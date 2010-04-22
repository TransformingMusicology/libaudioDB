extern "C" {
#include "audioDB_API.h"
}
#include "audioDB-internals.h"

static bool audiodb_check_header(adb_header_t *header) {
  /* FIXME: use syslog() or write to stderr or something to give the
     poor user some diagnostics. */
  if(header->magic == ADB_OLD_MAGIC) {
    return false;
  }
  if(header->magic != ADB_MAGIC) {
    return false;
  }
  if(header->version != ADB_FORMAT_VERSION) {
    return false;
  }
  if(header->headerSize != ADB_HEADER_SIZE) {
    return false;
  }
  return true;
}

static int audiodb_collect_keys(adb_t *adb) {
  char *key_table = 0;
  size_t key_table_length = 0;

  if(adb->header->length > 0) {
    unsigned nfiles = adb->header->numFiles;
    key_table_length = align_page_up(nfiles * ADB_FILETABLE_ENTRY_SIZE);
    malloc_and_fill_or_goto_error(char *, key_table, adb->header->fileTableOffset, key_table_length);
    for (unsigned int k = 0; k < nfiles; k++) {
      adb->keys->push_back(key_table + k*ADB_FILETABLE_ENTRY_SIZE);
      (*adb->keymap)[(key_table + k*ADB_FILETABLE_ENTRY_SIZE)] = k;
    }
    free(key_table);
  }

  return 0;

 error:
  maybe_free(key_table);
  return 1;
}

static int audiodb_collect_track_lengths(adb_t *adb) {
  uint32_t *track_table = 0;
  size_t track_table_length = 0;
  if(adb->header->length > 0) {
    unsigned nfiles = adb->header->numFiles;
    track_table_length = align_page_up(nfiles * ADB_TRACKTABLE_ENTRY_SIZE);
    malloc_and_fill_or_goto_error(uint32_t *, track_table, adb->header->trackTableOffset, track_table_length);
    off_t offset = 0;
    for (unsigned int k = 0; k < nfiles; k++) {
      uint32_t track_length = track_table[k];
      adb->track_lengths->push_back(track_length);
      adb->track_offsets->push_back(offset);
      offset += track_length * adb->header->dim * sizeof(double);
    }
    free(track_table);
  }

  return 0;

 error:
  maybe_free(track_table);
  return 1;
}

static int audiodb_init_rng(adb_t *adb) {
  gsl_rng *rng = gsl_rng_alloc(gsl_rng_mt19937);
  if(!rng) {
    return 1;
  }
  /* FIXME: maybe we should use a real source of entropy? */
  uint32_t seed = 0;
#ifdef WIN32
  seed = time(NULL);
#else
  struct timeval tv;
  if(gettimeofday(&tv, NULL) == -1) {
    return 1;
  }
  /* usec field should be less than than 2^20.  We want to mix the
     usec field, highly-variable, into the high bits of the seconds
     field, which will be static between closely-spaced runs.  -- CSR,
     2010-01-05 */
  seed = tv.tv_sec ^ (tv.tv_usec << 12);
#endif
  gsl_rng_set(rng, seed);
  adb->rng = rng;
  return 0;
}

adb_t *audiodb_open(const char *path, int flags) {
  adb_t *adb = 0;
  int fd = -1;

  flags &= (O_RDONLY|O_RDWR);
  fd = open(path, flags);
  if(fd == -1) {
    goto error;
  }
  if(acquire_lock(fd, flags == O_RDWR)) {
    goto error;
  }

  adb = (adb_t *) calloc(1, sizeof(adb_t));
  if(!adb) {
    goto error;
  }
  adb->fd = fd;
  adb->flags = flags;
  adb->path = (char *) malloc(1+strlen(path));
  if(!(adb->path)) {
    goto error;
  }
  strcpy(adb->path, path);

  adb->header = (adb_header_t *) malloc(sizeof(adb_header_t));
  if(!(adb->header)) {
    goto error;
  }
  if(read(fd, (char *) adb->header, ADB_HEADER_SIZE) != ADB_HEADER_SIZE) {
    goto error;
  }
  if(!audiodb_check_header(adb->header)) {
    goto error;
  }

  adb->keys = new std::vector<std::string>;
  if(!adb->keys) {
    goto error;
  }
  adb->keymap = new std::map<std::string,uint32_t>;
  if(!adb->keymap) {
    goto error;
  }
  if(audiodb_collect_keys(adb)) {
    goto error;
  }
  adb->track_lengths = new std::vector<uint32_t>;
  if(!adb->track_lengths) {
    goto error;
  }
  adb->track_lengths->reserve(adb->header->numFiles);
  adb->track_offsets = new std::vector<off_t>;
  if(!adb->track_offsets) {
    goto error;
  }
  adb->track_offsets->reserve(adb->header->numFiles);
  if(audiodb_collect_track_lengths(adb)) {
    goto error;
  }
  adb->cached_lsh = 0;
  if(audiodb_init_rng(adb)) {
    goto error;
  }
  return adb;

 error:
  if(adb) {
    maybe_free(adb->header);
    maybe_free(adb->path);
    if(adb->keys) {
      delete adb->keys;
    }
    if(adb->keymap) {
      delete adb->keymap;
    }
    if(adb->track_lengths) {
      delete adb->track_lengths;
    }
    if(adb->rng) {
      gsl_rng_free(adb->rng);
    }
    free(adb);
  }
  if(fd != -1) {
    close(fd);
  }
  return NULL;
}
