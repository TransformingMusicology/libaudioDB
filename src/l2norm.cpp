extern "C" {
#include "audioDB/audioDB_API.h"
}
#include "audioDB/audioDB-internals.h"

static int audiodb_l2norm_existing(adb_t *adb) {
  double *data_buffer = 0, *l2norm_buffer = 0;
  size_t data_buffer_size = align_page_up(adb->header->length);
  size_t nvectors = adb->header->length / (sizeof(double) * adb->header->dim);
  /* FIXME: this allocation of the vector data buffer will lose if we
   * ever turn the l2norm flag on when we have already inserted a
   * large number of vectors, as the malloc() will fail.  "Don't do
   * that, then" is one possible answer. */
  data_buffer = (double *) malloc(data_buffer_size);
  if (!data_buffer) {
    goto error;
  }
  lseek_set_or_goto_error(adb->fd, adb->header->dataOffset);
  read_or_goto_error(adb->fd, data_buffer, data_buffer_size);

  l2norm_buffer = (double *) malloc(nvectors * sizeof(double));
  if(!l2norm_buffer) {
    goto error;
  }
  audiodb_l2norm_buffer(data_buffer, adb->header->dim, nvectors, l2norm_buffer);
  lseek_set_or_goto_error(adb->fd, adb->header->l2normTableOffset);
  write_or_goto_error(adb->fd, l2norm_buffer, nvectors * sizeof(double));

  free(l2norm_buffer);
  free(data_buffer);

  return 0;

 error:
  if(data_buffer) {
    free(data_buffer);
  }
  if(l2norm_buffer) {
    free(l2norm_buffer);
  }
  return 1;
}

int audiodb_l2norm(adb_t *adb) {
  if(!(adb->flags & O_RDWR)) {
    return 1;
  }
  if(adb->header->flags & ADB_HEADER_FLAG_L2NORM) {
    /* non-error code for forthcoming backwards-compatibility
     * reasons */
    return 0;
  }
  if((!(adb->header->flags & ADB_HEADER_FLAG_REFERENCES)) && 
     (adb->header->length > 0)) {
    if(audiodb_l2norm_existing(adb)) {
      goto error;
    }
  }
  adb->header->flags |= ADB_HEADER_FLAG_L2NORM;
  return audiodb_sync_header(adb);

 error:
  return 1;
}
