#include "audioDB.h"

void audioDB::dump(const char* dbName){
  if(!dbH) {
    initTables(dbName, 0);
  }

  if(dbH->flags & O2_FLAG_LARGE_ADB){
    error("error: dump not supported for LARGE_ADB");
  }

  if((mkdir(output, S_IRWXU|S_IRWXG|S_IRWXO)) < 0) {
    error("error making output directory", output, "mkdir");
  }

  char *cwd = new char[PATH_MAX];

  if ((getcwd(cwd, PATH_MAX)) == 0) {
    error("error getting working directory", "", "getcwd");
  }

  if((chdir(output)) < 0) {
    error("error changing working directory", output, "chdir");
  }

  int fLfd, tLfd = 0, pLfd = 0, kLfd;
  FILE *fLFile, *tLFile = 0, *pLFile = 0, *kLFile;

  if ((fLfd = open("featureList.txt", O_CREAT|O_RDWR|O_EXCL, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)) < 0) {
    error("error creating featureList file", "featureList.txt", "open");
  }

  int times = dbH->flags & O2_FLAG_TIMES;
  if (times) {
    if ((tLfd = open("timesList.txt", O_CREAT|O_RDWR|O_EXCL, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)) < 0) {
      error("error creating timesList file", "timesList.txt", "open");
    }
  }

  int power = dbH->flags & O2_FLAG_POWER;
  if (power) {
    if ((pLfd = open("powerList.txt", O_CREAT|O_RDWR|O_EXCL, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)) < 0) {
      error("error creating powerList file", "powerList.txt", "open");
    }
  }

  if ((kLfd = open("keyList.txt", O_CREAT|O_RDWR|O_EXCL, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)) < 0) {
    error("error creating keyList file", "keyList.txt", "open");
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

  char *fName = new char[256];
  int ffd, pfd;
  FILE *tFile;
  unsigned pos = 0;
  lseek(dbfid, dbH->dataOffset, SEEK_SET);
  double *data_buffer;
  size_t data_buffer_size;
  for(unsigned k = 0; k < dbH->numFiles; k++) {
    fprintf(kLFile, "%s\n", fileTable + k*O2_FILETABLE_ENTRY_SIZE);
    snprintf(fName, 256, "%05d.features", k);
    if ((ffd = open(fName, O_CREAT|O_RDWR|O_EXCL, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)) < 0) {
      error("error creating feature file", fName, "open");
    }
    if ((write(ffd, &dbH->dim, sizeof(uint32_t))) < 0) {
      error("error writing dimensions", fName, "write");
    }

    /* FIXME: this repeated malloc()/free() of data buffers is
       inefficient. */
    data_buffer_size = trackTable[k] * dbH->dim * sizeof(double);

    {
      void *tmp = malloc(data_buffer_size);
      if (tmp == NULL) {
	error("error allocating data buffer");
      }
      data_buffer = (double *) tmp;
    }

    if ((read(dbfid, data_buffer, data_buffer_size)) != (ssize_t) data_buffer_size) {
      error("error reading data", fName, "read");
    }

    if ((write(ffd, data_buffer, data_buffer_size)) < 0) {
      error("error writing data", fName, "write");
    }

    free(data_buffer);

    fprintf(fLFile, "%s\n", fName);
    close(ffd);

    if (times) {
      snprintf(fName, 256, "%05d.times", k);
      tFile = fopen(fName, "w");
      for(unsigned i = 0; i < trackTable[k]; i++) {
        // KLUDGE: specifying 16 digits of precision after the decimal
        // point is (but check this!) sufficient to uniquely identify
        // doubles; however, that will cause ugliness, as that's
        // vastly too many for most values of interest.  Moving to %a
        // here and scanf() in the timesFile reading might fix this.
        // -- CSR, 2007-10-19
        fprintf(tFile, "%.16e\n", *(timesTable + 2*pos + 2*i));
      }
      fprintf(tFile, "%.16e\n", *(timesTable + 2*pos + 2*trackTable[k]-1));

      fprintf(tLFile, "%s\n", fName);
    }

    if (power) {
      uint32_t one = 1;
      snprintf(fName, 256, "%05d.power", k);
      if ((pfd = open(fName, O_CREAT|O_RDWR|O_EXCL, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)) < 0) {
	error("error creating power file", fName, "open");
      }
      if ((write(pfd, &one, sizeof(uint32_t))) < 0) {
	error("error writing one", fName, "write");
      }
      if ((write(pfd, powerTable + pos, trackTable[k] * sizeof(double))) < 0) {
	error("error writing data", fName, "write");
      }
      fprintf(pLFile, "%s\n", fName);
      close(pfd);
    } 

    pos += trackTable[k];
    std::cout << fileTable+k*O2_FILETABLE_ENTRY_SIZE << " " << trackTable[k] << std::endl;
  }

  FILE *scriptFile;
  scriptFile = fopen("restore.sh", "w");
  fprintf(scriptFile, "\
#! /bin/sh\n\
#\n\
# usage: AUDIODB=/path/to/audioDB sh ./restore.sh <newdb>\n\
\n\
if [ -z \"${AUDIODB}\" ]; then echo set AUDIODB variable; exit 1; fi\n\
if [ -z \"$1\" ]; then echo usage: $0 newdb; exit 1; fi\n\n\
\"${AUDIODB}\" -d \"$1\" -N --datasize=%d --ntracks=%d --datadim=%d\n",
          (int) ((dbH->timesTableOffset - dbH->dataOffset) / (1024*1024)),
          // fileTable entries (char[256]) are bigger than trackTable
          // (int), so the granularity of page aligning is finer.
          (int) ((dbH->trackTableOffset - dbH->fileTableOffset) / O2_FILETABLE_ENTRY_SIZE),
          (int) ceil(((double) (dbH->timesTableOffset - dbH->dataOffset)) / ((double) (dbH->dbSize - dbH->l2normTableOffset))));
  if(dbH->flags & O2_FLAG_L2NORM) {
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

  if((chdir(cwd)) < 0) {
    error("error changing working directory", cwd, "chdir");
  }

  fclose(fLFile);
  if(times) {
    fclose(tLFile);
  }
  if(power) {
    fclose(pLFile);
  }
  fclose(kLFile);
  delete[] fName;
    
  status(dbName);
}
