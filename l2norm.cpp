#include "audioDB.h"
extern "C" {
#include "audioDB_API.h"
#include "audioDB-internals.h"
}

static int audiodb_l2norm_existing(adb_t *adb) {
  double *data_buffer, *l2norm_buffer;
  adb_header_t *header = adb->header;
  size_t data_buffer_size = ALIGN_PAGE_UP(header->length);
  size_t nvectors = header->length / (sizeof(double) * header->dim);
  /* FIXME: this map of the vector data will lose if we ever turn the
   * l2norm flag on when we have already inserted a large number of
   * vectors, as the mmap() will fail.  "Don't do that, then" is one
   * possible answer. */
  mmap_or_goto_error(double *, data_buffer, header->dataOffset, data_buffer_size);
  l2norm_buffer = (double *) malloc(nvectors * sizeof(double));
  if(!l2norm_buffer) {
    goto error;
  }
  audiodb_l2norm_buffer(data_buffer, header->dim, nvectors, l2norm_buffer);
  if(lseek(adb->fd, adb->header->l2normTableOffset, SEEK_SET) == (off_t) -1) {
    goto error;
  }
  write_or_goto_error(adb->fd, l2norm_buffer, nvectors * sizeof(double));

  munmap(data_buffer, data_buffer_size);
  free(l2norm_buffer);

  return 0;

 error:
  maybe_munmap(data_buffer, data_buffer_size);
  if(l2norm_buffer) {
    free(l2norm_buffer);
  }
  return 1;
}

int audiodb_l2norm(adb_t *adb) {
  adb_header_t *header = adb->header;
  if(!(adb->flags & O_RDWR)) {
    return 1;
  }
  if(header->flags & O2_FLAG_L2NORM) {
    /* non-error code for forthcoming backwards-compatibility
     * reasons */
    return 0;
  }
  if((!(header->flags & O2_FLAG_LARGE_ADB)) && (header->length > 0)) {
    if(audiodb_l2norm_existing(adb)) {
      goto error;
    }
  }
  adb->header->flags |= O2_FLAG_L2NORM;
  return audiodb_sync_header(adb);

 error:
  return 1;
}
