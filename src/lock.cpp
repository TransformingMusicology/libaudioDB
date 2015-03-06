extern "C" {
#include "audioDB/audioDB_API.h"
}
#include "audioDB/audioDB-internals.h"

int acquire_lock(int fd, bool exclusive) {
#if !defined(WIN32)
  struct flock lock;
  int status;
  
  lock.l_type = exclusive ? F_WRLCK : F_RDLCK;
  lock.l_whence = SEEK_SET;
  lock.l_start = 0;
  lock.l_len = ADB_HEADER_SIZE;

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
#else
  /* _locking() only supports exclusive locks */
  int status;

 retry:
  status = _locking(fd, _LK_NBLCK, ADB_HEADER_SIZE);
  if(status) {
    Sleep(1000);
    goto retry;
  }
  return 0;
#endif
}

int divest_lock(int fd) {
#if !defined(WIN32)
  struct flock lock;

  lock.l_type = F_UNLCK;
  lock.l_whence = SEEK_SET;
  lock.l_start = 0;
  lock.l_len = ADB_HEADER_SIZE;

  return fcntl(fd, F_SETLKW, &lock);
#else
  return _locking(fd, _LK_UNLCK, ADB_HEADER_SIZE);
#endif
}
