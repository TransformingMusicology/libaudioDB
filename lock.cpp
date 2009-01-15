extern "C" {
#include "audioDB_API.h"
}
#include "audioDB-internals.h"

int acquire_lock(int fd, bool exclusive) {
  struct flock lock;
  int status;
  
  lock.l_type = exclusive ? F_WRLCK : F_RDLCK;
  lock.l_whence = SEEK_SET;
  lock.l_start = 0;
  lock.l_len = 0; /* "the whole file" */

 retry:
  do {
    status = fcntl(fd, F_SETLKW, &lock);
  } while (status != 0 && errno == EINTR);
  
  if (status) {
    if (errno == EAGAIN) {
      sleep(1);
      goto retry;
    } else {
      return status;
    }
  }
  return 0;
}

int divest_lock(int fd) {
  struct flock lock;

  lock.l_type = F_UNLCK;
  lock.l_whence = SEEK_SET;
  lock.l_start = 0;
  lock.l_len = 0;

  return fcntl(fd, F_SETLKW, &lock);
}
