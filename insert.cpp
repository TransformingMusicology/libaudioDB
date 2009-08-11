extern "C" {
#include "audioDB_API.h"
}
#include "audioDB-internals.h"

static bool audiodb_enough_data_space_free(adb_t *adb, off_t size) {
  adb_header_t *header = adb->header;
  if(header->flags & ADB_HEADER_FLAG_REFERENCES) {
    return true;
  } else {
    /* FIXME: timesTableOffset isn't necessarily the next biggest
     * offset after dataOffset.  Maybe make the offsets into an array
     * that we can iterate over... */
    return (header->timesTableOffset > 
            (header->dataOffset + header->length + size));
  }
}

static bool audiodb_enough_per_file_space_free(adb_t *adb) {
  /* FIXME: the comment above about the ordering of the tables applies
     here too. */
  adb_header_t *header = adb->header;
  off_t file_table_length = header->trackTableOffset - header->fileTableOffset;
  off_t track_table_length = header->dataOffset - header->trackTableOffset;
  int fmaxfiles = file_table_length / ADB_FILETABLE_ENTRY_SIZE;
  int tmaxfiles = track_table_length / ADB_TRACKTABLE_ENTRY_SIZE;
  /* maxfiles is the _minimum_ of the two.  Do not be confused... */
  int maxfiles = fmaxfiles > tmaxfiles ? tmaxfiles : fmaxfiles;
  if(header->flags & ADB_HEADER_FLAG_REFERENCES) {
    /* by default, these tables are created with the same size as the
     * fileTable (which should be called key_table); relying on that
     * always being the case, though, smacks of optimism, so instead
     * we code defensively... */
    off_t data_table_length = header->timesTableOffset - header->dataOffset;
    off_t times_table_length = header->powerTableOffset - header->timesTableOffset;
    off_t power_table_length = header->dbSize - header->powerTableOffset;
    int dmaxfiles = data_table_length / ADB_FILETABLE_ENTRY_SIZE;
    int timaxfiles = times_table_length / ADB_FILETABLE_ENTRY_SIZE;
    int pmaxfiles = power_table_length / ADB_FILETABLE_ENTRY_SIZE;
    /* ... even though it means a certain amount of tedium. */
    maxfiles = maxfiles > dmaxfiles ? dmaxfiles : maxfiles;
    maxfiles = maxfiles > timaxfiles ? timaxfiles : maxfiles;
    maxfiles = maxfiles > pmaxfiles ? pmaxfiles : maxfiles;
  }
  return (header->numFiles < (unsigned int) maxfiles);
}

/*
 * Hey, look, a comment.  Normally I wouldn't bother, as the code
 * should be self-documenting, but a lot of logic is concentrated in
 * this one place, so let's give an overview beforehand.  To insert a
 * datum into the database, we:
 *
 *  1. check write permission;
 *  2. check for enough space;
 *  3. check that datum->dim and adb->header->dim agree (or that the
 *     header dimension is zero, in which case write datum->dim to
 *     adb->header->dim).
 *  4. check for presence of datum->key in adb->keymap;
 *  5. check for consistency between power and ADB_HEADER_FLAG_POWER,
 *     and times and ADB_HEADER_FLAG_TIMES;
 *  6. write in data, power, times as appropriate; add to track
 *     and key tables too;
 *  7. if ADB_HEADER_FLAG_L2NORM and !ADB_HEADER_FLAG_REFERENCES,
 *     compute norms and fill in table;
 *  8. update adb->keys, adb->keymap, adb->track_lengths,
 *     adb->track_offsets and adb->header;
 *  9. sync adb->header with disk.
 *
 * Step 9 essentially commits the transaction; until we update
 * header->length, nothing will recognize the newly-written data.  In
 * principle, if it fails, we should roll back, which we can in fact
 * do on the assumption that nothing in step 8 can ever fail; on the
 * other hand, if it's failed, then it's unlikely that rolling back by
 * syncing the original header back to disk is going to work
 * desperately well.  We should perhaps take an operating-system lock
 * around step 9, so that we can't be interrupted part-way through
 * (except of course for SIGKILL, but if we're hit with that we will
 * always lose).
 */
static int audiodb_insert_datum_internal(adb_t *adb, adb_datum_internal_t *datum) {

  off_t size, offset, nfiles;
  double *l2norm_buffer = NULL;

  /* 1. check write permission; */
  if(!(adb->flags & O_RDWR)) {
    return 1;
  }
  /* 2. check for enough space; */
  size = sizeof(double) * datum->nvectors * datum->dim;
  if(!audiodb_enough_data_space_free(adb, size)) {
    return 1;
  }
  if(!audiodb_enough_per_file_space_free(adb)) {
    return 1;
  }
  /* 3. check that datum->dim and adb->header->dim agree (or that the
   *    header dimension is zero, in which case write datum->dim to
   *    adb->header->dim).
   */
  if(adb->header->dim == 0) {
    adb->header->dim = datum->dim;
  } else if (adb->header->dim != datum->dim) {
    return 1;
  }
  /* 4. check for presence of datum->key in adb->keymap; */
  if(adb->keymap->count(datum->key)) {
    /* not part of an explicit API/ABI, but we need a distinguished
       value in this circumstance to preserve somewhat wonky behaviour
       of audioDB::batchinsert. */
    return 2;
  }
  /* 5. check for consistency between power and ADB_HEADER_FLAG_POWER,
   *    and times and ADB_HEADER_FLAG_TIMES;
   */
  if((datum->power && !(adb->header->flags & ADB_HEADER_FLAG_POWER)) ||
     ((adb->header->flags & ADB_HEADER_FLAG_POWER) && !datum->power)) {
    return 1;
  }
  if(datum->times && !(adb->header->flags & ADB_HEADER_FLAG_TIMES)) {
    if(adb->header->numFiles == 0) {
      adb->header->flags |= ADB_HEADER_FLAG_TIMES;
    } else {
      return 1;
    }
  } else if ((adb->header->flags & ADB_HEADER_FLAG_TIMES) && !datum->times) {
    return 1;
  }
  /* 6. write in data, power, times as appropriate; add to track
   *    and key tables too;
   */
  offset = adb->header->length;
  nfiles = adb->header->numFiles;

  /* FIXME: checking for all these lseek()s */
  lseek_set_or_goto_error(adb->fd, adb->header->fileTableOffset + nfiles * ADB_FILETABLE_ENTRY_SIZE);
  write_or_goto_error(adb->fd, datum->key, strlen(datum->key)+1);
  lseek_set_or_goto_error(adb->fd, adb->header->trackTableOffset + nfiles * ADB_TRACKTABLE_ENTRY_SIZE);
  write_or_goto_error(adb->fd, &datum->nvectors, ADB_TRACKTABLE_ENTRY_SIZE);
  if(adb->header->flags & ADB_HEADER_FLAG_REFERENCES) {
    char cwd[PATH_MAX];
    char slash = '/';

    if(!getcwd(cwd, PATH_MAX)) {
      goto error;
    }
    lseek_set_or_goto_error(adb->fd, adb->header->dataOffset + nfiles * ADB_FILETABLE_ENTRY_SIZE);
    if(*((char *) datum->data) != '/') {
      write_or_goto_error(adb->fd, cwd, strlen(cwd));
      write_or_goto_error(adb->fd, &slash, 1);
    }
    write_or_goto_error(adb->fd, datum->data, strlen((const char *) datum->data)+1);
    if(datum->power) {
      lseek_set_or_goto_error(adb->fd, adb->header->powerTableOffset + nfiles * ADB_FILETABLE_ENTRY_SIZE);
      if(*((char *) datum->power) != '/') {
        write_or_goto_error(adb->fd, cwd, strlen(cwd));
        write_or_goto_error(adb->fd, &slash, 1);
      }
      write_or_goto_error(adb->fd, datum->power, strlen((const char *) datum->power)+1);
    }
    if(datum->times) {
      lseek_set_or_goto_error(adb->fd, adb->header->timesTableOffset + nfiles * ADB_FILETABLE_ENTRY_SIZE);
      if(*((char *) datum->times) != '/') {
        write_or_goto_error(adb->fd, cwd, strlen(cwd));
        write_or_goto_error(adb->fd, &slash, 1);
      }
      write_or_goto_error(adb->fd, datum->times, strlen((const char *) datum->times)+1);
    }
  } else {
    lseek_set_or_goto_error(adb->fd, adb->header->dataOffset + offset);
    write_or_goto_error(adb->fd, datum->data, sizeof(double) * datum->nvectors * datum->dim);
    if(datum->power) {
      lseek_set_or_goto_error(adb->fd, adb->header->powerTableOffset + offset / datum->dim);
      write_or_goto_error(adb->fd, datum->power, sizeof(double) * datum->nvectors);
    }
    if(datum->times) {
      lseek_set_or_goto_error(adb->fd, adb->header->timesTableOffset + offset / datum->dim * 2);
      write_or_goto_error(adb->fd, datum->times, sizeof(double) * datum->nvectors * 2);
    }
  }

  /* 7. if ADB_HEADER_FLAG_L2NORM and !ADB_HEADER_FLAG_REFERENCES,
   *    compute norms and fill in table;
   */
  if((adb->header->flags & ADB_HEADER_FLAG_L2NORM) &&
     !(adb->header->flags & ADB_HEADER_FLAG_REFERENCES)) {
    l2norm_buffer = (double *) malloc(datum->nvectors * sizeof(double));
    
    audiodb_l2norm_buffer((double *) datum->data, datum->dim, datum->nvectors, l2norm_buffer);
    lseek_set_or_goto_error(adb->fd, adb->header->l2normTableOffset + offset / datum->dim);
    write_or_goto_error(adb->fd, l2norm_buffer, sizeof(double) * datum->nvectors);
    free(l2norm_buffer);
    l2norm_buffer = NULL;
  }

  /* 8. update adb->keys, adb->keymap, adb->track_lengths,
   *    adb->track_offsets and adb->header;
   */
  adb->keys->push_back(datum->key);
  (*adb->keymap)[datum->key] = adb->header->numFiles;
  adb->track_lengths->push_back(datum->nvectors);
  adb->track_offsets->push_back(offset);
  adb->header->numFiles += 1;
  adb->header->length += sizeof(double) * datum->nvectors * datum->dim;

  /* 9. sync adb->header with disk. */
  return audiodb_sync_header(adb);

 error:
  maybe_free(l2norm_buffer);
  return 1;
}

int audiodb_insert_datum(adb_t *adb, const adb_datum_t *datum) {
  if(adb->header->flags & ADB_HEADER_FLAG_REFERENCES) {
    return 1;
  } else {
    adb_datum_internal_t d;
    d.nvectors = datum->nvectors;
    d.dim = datum->dim;
    d.key = datum->key;
    d.data = datum->data;
    d.times = datum->times;
    d.power = datum->power;
    return audiodb_insert_datum_internal(adb, &d);
  }
}

int audiodb_insert_reference(adb_t *adb, const adb_reference_t *reference) {
  if(!(adb->header->flags & ADB_HEADER_FLAG_REFERENCES)) {
    return 1;
  } else {
    adb_datum_internal_t d;
    struct stat st;
    int fd;
    off_t size;
    
    if((fd = open(reference->features, O_RDONLY)) == -1) {
      return 1;
    }
    if(fstat(fd, &st)) {
      goto error;
    }
    read_or_goto_error(fd, &(d.dim), sizeof(uint32_t));
    close(fd);
    fd = 0;
    size = st.st_size - sizeof(uint32_t);
    d.nvectors = size / (sizeof(double) * d.dim);
    d.data = (void *) reference->features;
    if(reference->power) {
      if(stat(reference->power, &st)) {
        return 1;
      }
    }
    d.power = (void *) reference->power;
    if(reference->times) {
      if(stat(reference->times, &st)) {
        return 1;
      }
    }
    d.times = (void *) reference->times;
    d.key = reference->key ? reference->key : reference->features;
    return audiodb_insert_datum_internal(adb, &d);
  error:
    if(fd) {
      close(fd);
    }
    return 1;
  }
}

int audiodb_really_free_datum(adb_datum_t *datum) {
  if(datum->data) {
    free(datum->data);
    datum->data = NULL;
  }
  if(datum->power) {
    free(datum->power);
    datum->power = NULL;
  }
  if(datum->times) {
    free(datum->times);
    datum->times = NULL;
  }
  return 0;
}

int audiodb_insert_create_datum(adb_insert_t *insert, adb_datum_t *datum) {
  int fd = 0;
  FILE *file = NULL;
  struct stat st;
  off_t size;

  datum->data = NULL;
  datum->power = NULL;
  datum->times = NULL;
  if((fd = open(insert->features, O_RDONLY)) == -1) {
    goto error;
  }
  if(fstat(fd, &st)) {
    goto error;
  }
  read_or_goto_error(fd, &(datum->dim), sizeof(uint32_t));
  size = st.st_size - sizeof(uint32_t);
  datum->nvectors = size / (sizeof(double) * datum->dim);
  datum->data = (double *) malloc(size);
  if(!datum->data) {
    goto error;
  }
  read_or_goto_error(fd, datum->data, size);
  close(fd);
  fd = 0;
  if(insert->power) {
    int dim;
    if((fd = open(insert->power, O_RDONLY)) == -1) {
      goto error;
    }
    if(fstat(fd, &st)) {
      goto error;
    }
    /* This cast is so non-trivial that it deserves a comment.
     *
     * The data types in this expression, left to right, are: off_t,
     * size_t, off_t, uint32_t.  The rules for conversions in
     * arithmetic expressions with mixtures of integral types are
     * essentially that the widest type wins, with unsigned types
     * winning on a tie-break.
     *
     * Because we are enforcing (through the use of sufficient
     * compiler flags, if necessary) that off_t be a (signed) 64-bit
     * type, the only variability in this set of types is in fact the
     * size_t.  On 32-bit machines, size_t is uint32_t and so the
     * coercions on both sides of the equality end up promoting
     * everything to int64_t, which is fine.  On 64-bit machines,
     * however, the left hand side is promoted to a uint64_t, while
     * the right hand side remains int64_t.
     *
     * The mixture of signed and unsigned types in comparisons is Evil
     * Bad and Wrong, and gcc complains about it.  (It's right to do
     * so, actually).  Of course in this case it will never matter
     * because of the particular relationships between all of these
     * numbers, so we just cast the left hand side to off_t, which
     * will do the right thing for us on all platforms.
     *
     * I hate C.
     *
     * Addendum: the above reasoning is skewered on Win32, where off_t
     * is apparently signed 32-bit always (i.e. no large-file
     * support).  So now, we cast datum->dim to size_t, so that our
     * types are the same on both sides.  I hate C even more.
     */
    if((st.st_size - sizeof(uint32_t)) != (size / (size_t) datum->dim)) {
      goto error;
    }
    read_or_goto_error(fd, &dim, sizeof(uint32_t));
    if(dim != 1) {
      goto error;
    }
    datum->power = (double *) malloc(size / datum->dim);
    if(!datum->power) {
      goto error;
    }
    read_or_goto_error(fd, datum->power, size / datum->dim);
    close(fd);
  }
  if(insert->times) {
    double t, *tp;
    if(!(file = fopen(insert->times, "r"))) {
      goto error;
    }
    datum->times = (double *) malloc(2 * size / datum->dim);
    if(!datum->times) {
      goto error;
    }
    if(fscanf(file, " %lf", &t) != 1) {
      goto error;
    }
    tp = datum->times;
    *tp++ = t;
    for(unsigned int n = 0; n < datum->nvectors - 1; n++) {
      if(fscanf(file, " %lf", &t) != 1) {
        goto error;
      }
      *tp++ = t;
      *tp++ = t;
    }
    if(fscanf(file, " %lf", &t) != 1) {
      goto error;
    }
    *tp = t;
    fclose(file);
  }
  datum->key = insert->key ? insert->key : insert->features;
  return 0;

 error:
  if(fd > 0) {
    close(fd);
  }
  if(file) {
    fclose(file);
  }
  audiodb_really_free_datum(datum);
  return 1;
}

int audiodb_insert(adb_t *adb, adb_insert_t *insert) {
  if(adb->header->flags & ADB_HEADER_FLAG_REFERENCES) {
    adb_reference_t *reference = insert;
    int err;
    err = audiodb_insert_reference(adb, reference);

    if(err == 2) {
      return 0;
    } else {
      return err;
    }
  } else {
    adb_datum_t datum;
    int err;

    if(audiodb_insert_create_datum(insert, &datum)) {
      return 1;
    }
    err = audiodb_insert_datum(adb, &datum);
    audiodb_really_free_datum(&datum);

    if(err == 2) {
      return 0;
    } else {
      return err;
    }
  }
}

int audiodb_batchinsert(adb_t *adb, adb_insert_t *insert, unsigned int size) {
  int err;
  for(unsigned int n = 0; n < size; n++) {
    if((err = audiodb_insert(adb, &(insert[n])))) {
      return err;
    }
  }
  return 0;
}
