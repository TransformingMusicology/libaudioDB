extern "C" {
#include "audioDB/audioDB_API.h"
}
#include "audioDB/audioDB-internals.h"

int audiodb_dump(adb_t *adb, const char *output) {
  char *fileTable = 0; /* key_table */
  double *timesTable = 0; /* timestamps_table */
  double *powerTable = 0; /* power_table */

  size_t fileTableLength = 0;
  size_t timesTableLength = 0;
  size_t powerTableLength = 0;

  char *featureFileNameTable = 0;
  char *powerFileNameTable = 0;
  char *timesFileNameTable = 0;
 
  char cwd[PATH_MAX];
  int directory_changed = 0;

  int fLfd = 0, tLfd = 0, pLfd = 0, kLfd = 0;
  FILE *fLFile = 0, *tLFile = 0, *pLFile = 0, *kLFile = 0;

  int times, power;

  char fName[256];
  int ffd, pfd;
  FILE *tFile;
  unsigned pos = 0;
  double *data_buffer;
  size_t data_buffer_size;
  FILE *scriptFile = 0;

  unsigned nfiles = adb->header->numFiles;

  if(adb->header->length > 0) {
    fileTableLength = align_page_up(nfiles * ADB_FILETABLE_ENTRY_SIZE);
    if(!(adb->header->flags & ADB_HEADER_FLAG_REFERENCES)) {
      off_t length = adb->header->length;
      unsigned dim = adb->header->dim;
      timesTableLength = align_page_up(2*length/dim);
      powerTableLength = align_page_up(length/dim);
    }

    malloc_and_fill_or_goto_error(char *, fileTable, adb->header->fileTableOffset, fileTableLength);
    if (adb->header->flags & ADB_HEADER_FLAG_REFERENCES) {
      malloc_and_fill_or_goto_error(char *, featureFileNameTable, adb->header->dataOffset, fileTableLength);
      malloc_and_fill_or_goto_error(char *, powerFileNameTable, adb->header->powerTableOffset, fileTableLength);
      malloc_and_fill_or_goto_error(char *, timesFileNameTable, adb->header->timesTableOffset, fileTableLength);
    } else {
      malloc_and_fill_or_goto_error(double *, powerTable, adb->header->powerTableOffset, powerTableLength);
      malloc_and_fill_or_goto_error(double *, timesTable, adb->header->timesTableOffset, timesTableLength);
    }
  }

  mkdir_or_goto_error(output);

  if ((getcwd(cwd, PATH_MAX)) == 0) {
    goto error;
  }

  /* FIXME: Hrm.  How does chdir(2) interact with threads?  Does each
   * thread have its own working directory? */
  if((chdir(output)) < 0) {
    goto error;
  }
  directory_changed = 1;

  if ((fLfd = open("featureList.txt", O_CREAT|O_RDWR|O_EXCL, ADB_CREAT_PERMISSIONS)) < 0) {
    goto error;
  }

  times = adb->header->flags & ADB_HEADER_FLAG_TIMES;
  if (times) {
    if ((tLfd = open("timesList.txt", O_CREAT|O_RDWR|O_EXCL, ADB_CREAT_PERMISSIONS)) < 0) {
      goto error;
    }
  }

  power = adb->header->flags & ADB_HEADER_FLAG_POWER;
  if (power) {
    if ((pLfd = open("powerList.txt", O_CREAT|O_RDWR|O_EXCL, ADB_CREAT_PERMISSIONS)) < 0) {
      goto error;
    }
  }

  if ((kLfd = open("keyList.txt", O_CREAT|O_RDWR|O_EXCL, ADB_CREAT_PERMISSIONS)) < 0) {
    goto error;
  }
  
  /* can these fail?  I sincerely hope not. */
  fLFile = fdopen(fLfd, "w");
  if (times) {
    tLFile = fdopen(tLfd, "w");
  }
  if (power) {
    pLFile = fdopen(pLfd, "w");
  }
  kLFile = fdopen(kLfd, "w");

  lseek_set_or_goto_error(adb->fd, adb->header->dataOffset);

  for(unsigned k = 0; k < nfiles; k++) {
    fprintf(kLFile, "%s\n", fileTable + k*ADB_FILETABLE_ENTRY_SIZE);
    if(adb->header->flags & ADB_HEADER_FLAG_REFERENCES) {
      char *featureFileName = featureFileNameTable+k*ADB_FILETABLE_ENTRY_SIZE;
      if(*featureFileName != '/') {
        goto error;
      }
      fprintf(fLFile, "%s\n", featureFileName);
      if(times) {
	char *timesFileName = timesFileNameTable + k*ADB_FILETABLE_ENTRY_SIZE;
	if(*timesFileName != '/') {
          goto error;
	}
	fprintf(tLFile, "%s\n", timesFileName);
      }
      if(power) {
	char *powerFileName = powerFileNameTable + k*ADB_FILETABLE_ENTRY_SIZE;
	if(*powerFileName != '/') {
          goto error;
	}
	fprintf(pLFile, "%s\n", powerFileName);
      }
    } else {
      snprintf(fName, 256, "%05d.features", k);
      if ((ffd = open(fName, O_CREAT|O_RDWR|O_EXCL, ADB_CREAT_PERMISSIONS)) < 0) {
        goto error;
      }
      write_or_goto_error(ffd, &(adb->header->dim), sizeof(uint32_t));
      
      /* FIXME: this repeated malloc()/free() of data buffers is
	 inefficient. */
      data_buffer_size = (*adb->track_lengths)[k] * adb->header->dim * sizeof(double);
      
      {
	void *tmp = malloc(data_buffer_size);
	if (tmp == NULL) {
          goto error;
	}
	data_buffer = (double *) tmp;
      }
      
      if ((read(adb->fd, data_buffer, data_buffer_size)) != (ssize_t) data_buffer_size) {
        goto error;
      }
      
      write_or_goto_error(ffd, data_buffer, data_buffer_size);
      
      free(data_buffer);
      
      fprintf(fLFile, "%s\n", fName);
      close(ffd);
      ffd = 0;

      if (times) {
	snprintf(fName, 256, "%05d.times", k);
	tFile = fopen(fName, "w");
	for(unsigned i = 0; i < (*adb->track_lengths)[k]; i++) {
	  // KLUDGE: specifying 16 digits of precision after the decimal
	  // point is (but check this!) sufficient to uniquely identify
	  // doubles; however, that will cause ugliness, as that's
	  // vastly too many for most values of interest.  Moving to %a
	  // here and scanf() in the timesFile reading might fix this.
	  // -- CSR, 2007-10-19
	  fprintf(tFile, "%.16e\n", *(timesTable + 2*pos + 2*i));
	}
	fprintf(tFile, "%.16e\n", *(timesTable + 2*pos + 2*(*adb->track_lengths)[k]-1));
        fclose(tFile);
	
	fprintf(tLFile, "%s\n", fName);
      }
      
      if (power) {
	uint32_t one = 1;
	snprintf(fName, 256, "%05d.power", k);
	if ((pfd = open(fName, O_CREAT|O_RDWR|O_EXCL, ADB_CREAT_PERMISSIONS)) < 0) {
          goto error;
	}
        write_or_goto_error(pfd, &one, sizeof(uint32_t));
        write_or_goto_error(pfd, powerTable + pos, (*adb->track_lengths)[k] * sizeof(double));
	fprintf(pLFile, "%s\n", fName);
	close(pfd);
        pfd = 0;
      } 
      
      pos += (*adb->track_lengths)[k];
      std::cout << fileTable+k*ADB_FILETABLE_ENTRY_SIZE << " " << (*adb->track_lengths)[k] << std::endl;
    }
  }

  scriptFile = fopen("restore.sh", "w");
  fprintf(scriptFile, "\
#! /bin/sh\n\
#\n\
# usage: AUDIODB=/path/to/audioDB sh ./restore.sh <newdb>\n\
\n\
if [ -z \"${AUDIODB}\" ]; then echo set AUDIODB variable; exit 1; fi\n\
if [ -z \"$1\" ]; then echo usage: $0 newdb; exit 1; fi\n\n\
\"${AUDIODB}\" -d \"$1\" -N --datasize=%d --ntracks=%d --datadim=%d\n",
          (int) ((adb->header->timesTableOffset - adb->header->dataOffset) / (1024*1024)),
          // fileTable entries (char[256]) are bigger than trackTable
          // (int), so the granularity of page aligning is finer.
          (int) ((adb->header->trackTableOffset - adb->header->fileTableOffset) / ADB_FILETABLE_ENTRY_SIZE),
          (int) ceil(((double) (adb->header->timesTableOffset - adb->header->dataOffset)) / ((double) (adb->header->dbSize - adb->header->l2normTableOffset))));
  if(adb->header->flags & ADB_HEADER_FLAG_L2NORM) {
    fprintf(scriptFile, "\"${AUDIODB}\" -d \"$1\" -L\n");
  }
  if(power) {
    fprintf(scriptFile, "\"${AUDIODB}\" -d \"$1\" -P\n");
  }
  fprintf(scriptFile, "\"${AUDIODB}\" -d \"$1\" -B -F featureList.txt -K keyList.txt");
  if(times) {
    fprintf(scriptFile, " -T timesList.txt");
  }
  if(power) {
    fprintf(scriptFile, " -W powerList.txt");
  }
  fprintf(scriptFile, "\n");
  fclose(scriptFile);

  fclose(fLFile);
  if(times) {
    fclose(tLFile);
  }
  if(power) {
    fclose(pLFile);
  }
  fclose(kLFile);
    
  maybe_free(fileTable);
  maybe_free(timesTable);
  maybe_free(powerTable);
  maybe_free(featureFileNameTable);
  maybe_free(timesFileNameTable);
  maybe_free(powerFileNameTable);

  if((chdir(cwd)) < 0) {
    /* don't goto error because the error handling will try to
     * chdir() */
    return 1;
  }

  return 0;

 error:
  if(fLFile) {
    fclose(fLFile);
  } else if(fLfd) {
    close(fLfd);
  }
  if(tLFile) {
    fclose(tLFile);
  } else if(tLfd) {
    close(fLfd);
  }
  if(pLFile) {
    fclose(pLFile);
  } else if(pLfd) {
    close(pLfd);
  }
  if(kLFile) {
    fclose(kLFile);
  } else if(kLfd) {
    close(kLfd);
  }
  if(scriptFile) {
    fclose(scriptFile);
  }

  maybe_free(fileTable);
  maybe_free(timesTable);
  maybe_free(powerTable);
  maybe_free(featureFileNameTable);
  maybe_free(timesFileNameTable);
  maybe_free(powerFileNameTable);

  if(directory_changed) {
    int gcc_warning_workaround = chdir(cwd);
    directory_changed = gcc_warning_workaround;
  }
  return 1;
}
