#include "audioDB.h"

#include <gsl/gsl_sf.h>
#include <gsl/gsl_rng.h>

static
double yfun(double d) {
  return gsl_sf_log(d) - gsl_sf_psi(d);
}

static
double yinv(double y) {
  double a = 1.0e-5;
  double b = 1000.0;

  double ay = yfun(a);
  double by = yfun(b);

  double c = 0;
  double cy;

  /* FIXME: simple binary search; there's probably some clever solver
     in gsl somewhere which is less sucky. */
  while ((b - a) > 1.0e-5) {
    c = (a + b) / 2;
    cy = yfun(c);
    if (cy > y) {
      a = c;
      ay = cy;
    } else {
      b = c;
      by = cy;
    }
  }

  return c;
}

unsigned audioDB::random_track(unsigned *propTable, unsigned total) {
  /* FIXME: make this O(1) by using the alias-rejection method, or
     some other sensible method of sampling from a discrete
     distribution. */
  double thing = gsl_rng_uniform(rng);
  unsigned sofar = 0;
  for (unsigned int i = 0; i < dbH->numFiles; i++) {
    sofar += propTable[i];
    if (thing < ((double) sofar / (double) total)) {
      return i;
    }
  }
  error("fell through in random_track()");

}

void audioDB::sample(const char *dbName) {
  initTables(dbName, 0);
  if(dbH->flags & O2_FLAG_LARGE_ADB){
    error("error: sample not yet supported for LARGE_ADB");
  }
    
  off_t *trackOffsetTable = new off_t[dbH->numFiles];
  unsigned cumTrack=0;
  for(unsigned int k = 0; k < dbH->numFiles; k++){
    trackOffsetTable[k] = cumTrack;
    cumTrack += trackTable[k] * dbH->dim;
  }

  unsigned *propTable = new unsigned[dbH->numFiles];
  unsigned total = 0;
  unsigned count = 0;

  for (unsigned int i = 0; i < dbH->numFiles; i++) {
    /* what kind of a stupid language doesn't have binary max(), let
       alone nary? */
    unsigned int prop = trackTable[i] - sequenceLength + 1;
    prop = prop > 0 ? prop : 0;
    if (prop > 0) 
      count++;
    propTable[i] = prop;
    total += prop;
  }

  if (total == 0) {
    error("no sequences of this sequence length in the database", dbName);
  }

  unsigned int vlen = dbH->dim * sequenceLength;
  double *v1 = new double[vlen];
  double *v2 = new double[vlen];
  double v1norm, v2norm, v1v2;

  double sumdist = 0;
  double sumlogdist = 0;

  unsigned key_index = 0;
  if(query_from_key) {
    /* naughty use of internals here.  When this is part of the API,
       it will be a legitimate use of internals.  -- CSR,
       2010-01-05 */
    key_index = (*adb->keymap)[key];
    if(propTable[key_index] == 0) {
      error("no samples of this length possible for key");
    }
  }
  for (unsigned int i = 0; i < nsamples;) {
    unsigned track1 = 0;
    if(query_from_key) {
      track1 = key_index;
    } else {
      track1 = random_track(propTable, total);
    }
    unsigned track2 = random_track(propTable, total);

    if(track1 == track2)
      continue;

    unsigned i1 = gsl_rng_uniform_int(rng, propTable[track1]);
    unsigned i2 = gsl_rng_uniform_int(rng, propTable[track2]);

    VERB_LOG(1, "%d %d, %d %d | ", track1, i1, track2, i2);

    /* FIXME: this seeking, reading and distance calculation should
       share more code with the query loop */
    if(lseek(dbfid, dbH->dataOffset + trackOffsetTable[track1] * sizeof(double) + i1 * dbH->dim * sizeof(double), SEEK_SET) == (off_t) -1) {
      error("seek failure", "", "lseek");
    }
    CHECKED_READ(dbfid, v1, dbH->dim * sequenceLength * sizeof(double));

    if(lseek(dbfid, dbH->dataOffset + trackOffsetTable[track2] * sizeof(double) + i2 * dbH->dim * sizeof(double), SEEK_SET) == (off_t) -1) {
      error("seek failure", "", "lseek");
    }
    CHECKED_READ(dbfid, v2, dbH->dim * sequenceLength * sizeof(double));

    v1norm = 0;
    v2norm = 0;
    v1v2 = 0;

    for (unsigned int j = 0; j < vlen; j++) {
      v1norm += v1[j]*v1[j];
      v2norm += v2[j]*v2[j];
      v1v2 += v1[j]*v2[j];
    }

    /* FIXME: we must deal with infinities better than this; there
       could be all sorts of NaNs from arbitrary features.  Best
       include power thresholds or something... */
    if(isfinite(v1norm) && isfinite(v2norm) && isfinite(v1v2)) {

      VERB_LOG(1, "%f %f %f | ", v1norm, v2norm, v1v2);
      /* assume normalizedDistance == true for now */
      /* FIXME: not convinced that the statistics we calculated in
	 TASLP paper are technically valid for normalizedDistance */

      double dist = 2 - 2 * v1v2 / sqrt(v1norm * v2norm);
      // double dist = v1norm + v2norm - 2*v1v2;
      
      VERB_LOG(1, "%f %f\n", dist, log(dist));
      sumdist += dist;
      sumlogdist += log(dist);
      i++;
    } else {
      VERB_LOG(1, "infinity/NaN found: %f %f %f\n", v1norm, v2norm, v1v2);
    }
  }

  /* FIXME: the mean isn't really what we should be reporting here */
  unsigned meanN = total / count;

  double sigma2 = sumdist / (sequenceLength * dbH->dim * nsamples);
  double d = 2 * yinv(log(sumdist/nsamples) - sumlogdist/nsamples);

  std::cout << "Summary statistics" << std::endl;
  std::cout << "number of samples: " << nsamples << std::endl;
  std::cout << "sum of distances (S): " << sumdist << std::endl;
  std::cout << "sum of log distances (L): " << sumlogdist << std::endl;

  /* FIXME: we'll also want some more summary statistics based on
     propTable, for the minimum-of-X estimate */
  std::cout << "mean number of applicable sequences (N): " << meanN << std::endl;
  std::cout << std::endl;
  std::cout << "Estimated parameters" << std::endl;
  std::cout << "sigma^2: " << sigma2 << "; ";
  std::cout << "Msigma^2: " << sumdist / nsamples << std::endl;
  std::cout << "d: " << d << std::endl;

  double logw = (2 / d) * gsl_sf_log(-gsl_sf_log(0.99));
  double logxthresh = gsl_sf_log(sumdist / nsamples) + logw
    - (2 / d) * gsl_sf_log(meanN)
    - gsl_sf_log(d/2)
    - (2 / d) * gsl_sf_log(2 / d)
    + (2 / d) * gsl_sf_lngamma(d / 2);

  std::cout << "track xthresh: " << exp(logxthresh) << std::endl;

  delete[] propTable;
  delete[] v1;
  delete[] v2;
}
