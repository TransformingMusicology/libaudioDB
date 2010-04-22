extern "C" {
#include "audioDB_API.h"
}
#include "audioDB-internals.h"
#include "accumulators.h"

/* 0. if the datum in the spec is sufficiently NULL, do db x db
   sampling;

   1. if accumulation is DATABASE, do basically the current sampling
   with track1 being the datum; 
   
   2. if accumulation is PER_TRACK, sample n points per db track
   rather than n points overall.
   
   3. if accumulation is ONE_TO_ONE, ponder hard.
*/
adb_query_results_t *audiodb_sample_spec(adb_t *adb, const adb_query_spec_t *qspec) {
  adb_qstate_internal_t qstate = {0};
  /* FIXME: this paragraph is the same as in audiodb_query_spec() */
  qstate.allowed_keys = new std::set<std::string>;
  adb_query_results_t *results;
  if(qspec->refine.flags & ADB_REFINE_INCLUDE_KEYLIST) {
    for(unsigned int k = 0; k < qspec->refine.include.nkeys; k++) {
      qstate.allowed_keys->insert(qspec->refine.include.keys[k]);
    }
  } else {
    for(unsigned int k = 0; k < adb->header->numFiles; k++) {
      qstate.allowed_keys->insert((*adb->keys)[k]);
    }
  }
  if(qspec->refine.flags & ADB_REFINE_EXCLUDE_KEYLIST) {
    for(unsigned int k = 0; k < qspec->refine.exclude.nkeys; k++) {
      qstate.allowed_keys->erase(qspec->refine.exclude.keys[k]);
    }
  }

  /* FIXME: this paragraph is the same as in audiodb_query_spec(). */
  switch(qspec->params.distance) {
  case ADB_DISTANCE_DOT_PRODUCT:
    switch(qspec->params.accumulation) {
    case ADB_ACCUMULATION_DB:
      qstate.accumulator = new DBAccumulator<adb_result_dist_gt>(qspec->params.npoints);
      break;
    case ADB_ACCUMULATION_PER_TRACK:
      qstate.accumulator = new PerTrackAccumulator<adb_result_dist_gt>(qspec->params.npoints, qspec->params.ntracks);
      break;
    case ADB_ACCUMULATION_ONE_TO_ONE:
      qstate.accumulator = new NearestAccumulator<adb_result_dist_gt>();
      break;
    default:
      goto error;
    }
    break;
  case ADB_DISTANCE_EUCLIDEAN_NORMED:
  case ADB_DISTANCE_EUCLIDEAN:
    switch(qspec->params.accumulation) {
    case ADB_ACCUMULATION_DB:
      qstate.accumulator = new DBAccumulator<adb_result_dist_lt>(qspec->params.npoints);
      break;
    case ADB_ACCUMULATION_PER_TRACK:
      qstate.accumulator = new PerTrackAccumulator<adb_result_dist_lt>(qspec->params.npoints, qspec->params.ntracks);
      break;
    case ADB_ACCUMULATION_ONE_TO_ONE:
      qstate.accumulator = new NearestAccumulator<adb_result_dist_lt>();
      break;
    default:
      goto error;
    }
    break;
  default:
    goto error;
  }

  if(audiodb_sample_loop(adb, qspec, &qstate)) {
    goto error;
  }

  results = qstate.accumulator->get_points();

  delete qstate.accumulator;
  delete qstate.allowed_keys;

  return results;

 error:
  if(qstate.accumulator)
    delete qstate.accumulator;
  if(qstate.allowed_keys)
    delete qstate.allowed_keys;
  return NULL;
}

uint32_t audiodb_random_track(adb_t *adb, uint32_t *propTable, unsigned total) {
  /* FIXME: make this O(1) by using the alias-rejection method, or
     some other sensible method of sampling from a discrete
     distribution. */
  double thing = gsl_rng_uniform(adb->rng);
  unsigned sofar = 0;
  for (unsigned int i = 0; i < adb->header->numFiles; i++) {
    sofar += propTable[i];
    if (thing < ((double) sofar / (double) total)) {
      return i;
    }
  }
  /* can't happen */
  return (uint32_t) -1;
}

int audiodb_sample_loop(adb_t *adb, const adb_query_spec_t *spec, adb_qstate_internal_t *qstate) {
  /* FIXME: all eerily reminiscent of audiodb_query_loop() */
  double *query, *query_data;
  adb_qpointers_internal_t qpointers = {0}, dbpointers = {0};

  if(adb->header->flags & ADB_HEADER_FLAG_REFERENCES) {
    /* FIXME: someday */
    return 1;
  }

  if(spec->qid.datum) {
    if(audiodb_query_spec_qpointers(adb, spec, &query_data, &query, &qpointers)) {
      return 1;
    }
  }

  /*
  if(audiodb_set_up_dbpointers(adb, spec, &dbpointers)) {
    return 1;
  }
  */

  /* three cases: 

     1. datum is null:  Select two random tracks and two
     offsets into those tracks, compute distance, and add to
     accumulator.

     2. datum is not null and

        (a) ADB_QID_FLAG_EXHAUSTIVE is set: select one random track
        and offset, and one offset into datum, compute distance, add
        to accumulator.

        (b) ADB_QID_FLAG_EXHAUSTIVE is not set: select one random
        track and offset, compute distance with datum, add to
        accumulator.

     That all sounds simple, right?  Oh, but we also need to take care
     of refinement: not just hop sizes, but power thresholding, track
     inclusion, duration ratio (theoretically), radius thresholding
     (no, that doesn't make sense).

     Also, for ADB_ACCUMULATE_PER_TRACK, we need to do effectively
     case 2 for each of the allowed keys.

  */

  if(spec->refine.flags & ADB_REFINE_RADIUS) {
    /* doesn't make sense */
    return 1;
  }

  if(spec->refine.flags & ADB_REFINE_DURATION_RATIO) {
    /* does make sense in principle but my brain is too small today */
    return -1;
  }

  if(spec->params.accumulation != ADB_ACCUMULATION_DB) {
    /* likewise, brain too small */
    return -1;
  }

  uint32_t qhop, ihop;
  if(spec->refine.flags & ADB_REFINE_HOP_SIZE) {
    qhop = spec->refine.qhopsize;
    qhop = qhop ? qhop : 1;
    ihop = spec->refine.ihopsize;
    ihop = ihop ? ihop : 1;
  } else {
    qhop = 1;
    ihop = 1;
  }

  uint32_t total = 0;
  uint32_t *props = new uint32_t[adb->header->numFiles];

  std::set<std::string>::iterator keys_end = qstate->allowed_keys->end();
  for(uint32_t i = 0; i < adb->header->numFiles; i++) {
    uint32_t prev = i == 0 ? 0 : props[i-1];
    if(qstate->allowed_keys->find((*adb->keys)[i]) == keys_end) {
      props[i] = prev;
    } else {
      uint32_t add = (*adb->track_lengths)[i] - spec->qid.sequence_length + 1;
      props[i] = prev + add > 0 ? add : 0;
      total += add;
    }
  }

  double *v1 = NULL;
  double *v2 = new double[adb->header->dim * spec->qid.sequence_length];

  if(!spec->qid.datum) {
    v1 = new double[adb->header->dim * spec->qid.sequence_length];
  }

  uint32_t i1, i2, track1 = 0, track2;

  uint32_t npoints = spec->params.npoints;
  for(uint32_t nsamples = 0; nsamples < npoints;) {

    if(spec->qid.datum) {
      if(spec->qid.flags & ADB_QID_FLAG_EXHAUSTIVE) {
        i1 = qhop * gsl_rng_uniform_int(adb->rng, (qpointers.nvectors - spec->qid.sequence_length + qhop - 1) / qhop);
        v1 = query + i1 * adb->header->dim;
      } else {
        i1 = spec->qid.sequence_start;
        v1 = query;
      }
      track2 = audiodb_random_track(adb, props, total);
      i2 = ihop * gsl_rng_uniform_int(adb->rng, (props[track2] + ihop - 1) / ihop);
      off_t offset2 = (*adb->track_offsets)[track2] + i2 * adb->header->dim * sizeof(double);
      uint32_t length = adb->header->dim * spec->qid.sequence_length * sizeof(double);
      lseek_set_or_goto_error(adb->fd, adb->header->dataOffset + offset2);
      read_or_goto_error(adb->fd, v2, length);
    } else {
      /* FIXME: Theoretically we should pass ihop here, but that's a
         small correction that I'm ignoring for now */
      track1 = audiodb_random_track(adb, props, total);
      track2 = audiodb_random_track(adb, props, total);
      if(track1 == track2) {
        continue;
      }

      i1 = ihop * gsl_rng_uniform_int(adb->rng, (props[track1] + ihop - 1) / ihop);
      i2 = ihop * gsl_rng_uniform_int(adb->rng, (props[track2] + ihop - 1) / ihop);

      off_t offset1 = (*adb->track_offsets)[track1] + i1 * adb->header->dim * sizeof(double);
      off_t offset2 = (*adb->track_offsets)[track2] + i2 * adb->header->dim * sizeof(double);
      uint32_t length = adb->header->dim * spec->qid.sequence_length * sizeof(double);
      lseek_set_or_goto_error(adb->fd, adb->header->dataOffset + offset1);
      read_or_goto_error(adb->fd, v1, length);
      lseek_set_or_goto_error(adb->fd, adb->header->dataOffset + offset2);
      read_or_goto_error(adb->fd, v2, length);
    }

    /* FIXME: replace at least the norms with dbpointers */
    double v1norm = 0;
    double v2norm = 0;
    double v1v2 = 0;
    double dist = 0;

    uint32_t vlen = spec->qid.sequence_length * adb->header->dim;
    for (unsigned int j = 0; j < vlen; j++) {
      v1norm += v1[j]*v1[j];
      v2norm += v2[j]*v2[j];
      v1v2 += v1[j]*v2[j];
    }

    /* FIXME: do power thresholding test */
    if(isfinite(v1norm) && isfinite(v2norm) && isfinite(v1v2)) {
      switch(spec->params.distance) {
      case ADB_DISTANCE_EUCLIDEAN_NORMED:
        dist = 2 - 2 * v1v2 / sqrt(v1norm * v2norm);
        break;
      case ADB_DISTANCE_EUCLIDEAN:
        dist = v1norm + v2norm - 2 * v1v2;
        break;
      case ADB_DISTANCE_DOT_PRODUCT:
        dist = v1v2;
        break;
      }
    } else {
      continue;
    }

    adb_result_t r;
    r.qkey = spec->qid.datum ? "" : (*adb->keys)[track1].c_str();
    r.ikey = (*adb->keys)[track2].c_str();
    r.dist = dist;
    r.qpos = i1;
    r.ipos = i2;
    qstate->accumulator->add_point(&r);
    nsamples++;
  }

  if(!spec->qid.datum) {
    delete[] v1;
  }
  delete[] v2;

  return 0;

 error:
  if(!spec->qid.datum) {
    delete[] v1;
  }
  delete[] v2;
  return 17;
}
