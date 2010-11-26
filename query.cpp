extern "C" {
#include "audioDB_API.h"
}
#include "audioDB-internals.h"
#include "accumulators.h"

bool audiodb_powers_acceptable(const adb_query_refine_t *r, double p1, double p2) {
  if (r->flags & ADB_REFINE_ABSOLUTE_THRESHOLD) {
    if ((p1 < r->absolute_threshold) || (p2 < r->absolute_threshold)) {
      return false;
    }
  }
  if (r->flags & ADB_REFINE_RELATIVE_THRESHOLD) {
    if (fabs(p1-p2) > fabs(r->relative_threshold)) {
      return false;
    }
  }
  return true;
}

adb_query_results_t *audiodb_query_spec(adb_t *adb, const adb_query_spec_t *qspec) {
  adb_qstate_internal_t qstate = {0};
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
  
  if((qspec->refine.flags & ADB_REFINE_RADIUS) && audiodb_index_exists(adb->path, qspec->refine.radius, qspec->qid.sequence_length)) {
    if(audiodb_index_query_loop(adb, qspec, &qstate) < 0) {
      goto error;
    }
  } else {
    if(audiodb_query_loop(adb, qspec, &qstate)) {
      goto error;
    }
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

int audiodb_query_free_results(adb_t *adb, const adb_query_spec_t *spec, adb_query_results_t *rs) {
  free(rs->results);
  free(rs);
  return 0;
}

/* FIXME: we should check the return values from allocation */
static void audiodb_initialize_arrays(adb_t *adb, const adb_query_spec_t *spec, int track, unsigned int numVectors, double *query, double *data_buffer, double **D, double **DD) {
  unsigned int j, k, l, w;
  double *dp, *qp, *sp;

  const unsigned wL = spec->qid.sequence_length;

  for(j = 0; j < numVectors; j++) {
    // Sum products matrix
    D[j] = new double[(*adb->track_lengths)[track]]; 
    // Matched filter matrix
    DD[j]=new double[(*adb->track_lengths)[track]];
  }

  // Dot product
  for(j = 0; j < numVectors; j++)
    for(k = 0; k < (*adb->track_lengths)[track]; k++){
      qp = query + j * adb->header->dim;
      sp = data_buffer + k * adb->header->dim;
      DD[j][k] = 0.0; // Initialize matched filter array
      dp = &D[j][k];  // point to correlation cell j,k
      *dp = 0.0;      // initialize correlation cell
      l = adb->header->dim;         // size of vectors
      while(l--)
        *dp += *qp++ * *sp++;
    }
  
  double* spd;
  if(!(spec->refine.flags & ADB_REFINE_HOP_SIZE)) {
    for(w = 0; w < wL; w++) {
      for(j = 0; j < numVectors - w; j++) { 
        sp = DD[j];
        spd = D[j+w] + w;
        k = (*adb->track_lengths)[track] - w;
	while(k--)
	  *sp++ += *spd++;
      }
    }
  } else {
    uint32_t qhop = spec->refine.qhopsize;
    qhop = qhop ? qhop : 1;
    uint32_t ihop = spec->refine.ihopsize;
    ihop = ihop ? ihop : 1;
    for(w = 0; w < wL; w++) {
      for(j = 0; j < numVectors - w; j += qhop) {
        sp = DD[j];
        spd = D[j+w]+w;
        for(k = 0; k < (*adb->track_lengths)[track] - w; k += ihop) {
          *sp += *spd;
          sp += ihop;
          spd += ihop;
        }
      }
    }
  }
}

static void audiodb_delete_arrays(int track, unsigned int numVectors, double **D, double **DD) {
  if(D != NULL) {
    for(unsigned int j = 0; j < numVectors; j++) {
      delete[] D[j];
    }
  }
  if(DD != NULL) {
    for(unsigned int j = 0; j < numVectors; j++) {
      delete[] DD[j];
    }
  }
}

int audiodb_read_data(adb_t *adb, int trkfid, int track, double **data_buffer_p, size_t *data_buffer_size_p) {
  uint32_t track_length = (*adb->track_lengths)[track];
  size_t track_size = track_length * sizeof(double) * adb->header->dim;
  if (track_size > *data_buffer_size_p) {
    if(*data_buffer_p) {
      free(*data_buffer_p);
    }
    { 
      *data_buffer_size_p = track_size;
      void *tmp = malloc(track_size);
      if (tmp == NULL) {
        goto error;
      }
      *data_buffer_p = (double *) tmp;
    }
  }

  read_or_goto_error(trkfid, *data_buffer_p, track_size);
  return 0;

 error:
  return 1;
}

int audiodb_track_id_datum(adb_t *adb, uint32_t track_id, adb_datum_t *d) {
  off_t track_offset = (*adb->track_offsets)[track_id];
  if(adb->header->flags & ADB_HEADER_FLAG_REFERENCES) {
    /* create a reference/insert, then use adb_insert_create_datum() */
    adb_reference_t reference = {0};
    char features[ADB_MAXSTR], power[ADB_MAXSTR], times[ADB_MAXSTR];
    lseek_set_or_goto_error(adb->fd, adb->header->dataOffset + track_id * ADB_FILETABLE_ENTRY_SIZE);
    read_or_goto_error(adb->fd, features, ADB_MAXSTR);
    reference.features = features;
    if(adb->header->flags & ADB_HEADER_FLAG_POWER) {
      lseek_set_or_goto_error(adb->fd, adb->header->powerTableOffset + track_id * ADB_FILETABLE_ENTRY_SIZE);
      read_or_goto_error(adb->fd, power, ADB_MAXSTR);
      reference.power = power;
    }
    if(adb->header->flags & ADB_HEADER_FLAG_TIMES) {
      lseek_set_or_goto_error(adb->fd, adb->header->timesTableOffset + track_id * ADB_FILETABLE_ENTRY_SIZE);
      read_or_goto_error(adb->fd, times, ADB_MAXSTR);
      reference.times = times;
    }
    return audiodb_insert_create_datum(&reference, d);
  } else {
    /* initialize from sources of data that we already have */
    d->nvectors = (*adb->track_lengths)[track_id];
    d->dim = adb->header->dim;
    d->key = (*adb->keys)[track_id].c_str();
    /* read out stuff from the database tables */
    malloc_and_fill_or_goto_error(double *, d->data, adb->header->dataOffset + track_offset, d->nvectors * d->dim * sizeof(double));
    if(adb->header->flags & ADB_HEADER_FLAG_POWER) {
      malloc_and_fill_or_goto_error(double *, d->power, adb->header->powerTableOffset + track_offset / d->dim, d->nvectors * sizeof(double));
    } else {
      d->power = NULL;
    }
    if(adb->header->flags & ADB_HEADER_FLAG_TIMES) {
      malloc_and_fill_or_goto_error(double *, d->times, adb->header->timesTableOffset + 2 * track_offset / d->dim, 2 * d->nvectors * sizeof(double));
    } else {
      d->times = NULL;
    }
    return 0;
  }
 error:
  audiodb_really_free_datum(d);
  return 1;
}

int audiodb_datum_qpointers(adb_datum_t *d, uint32_t sequence_length, double **vector_data, double **vector, adb_qpointers_internal_t *qpointers) {
  uint32_t nvectors = d->nvectors;

  qpointers->nvectors = nvectors;

  size_t vector_size = nvectors * sizeof(double) * d->dim;
  *vector_data = new double[vector_size / sizeof(double) ];
  memcpy(*vector_data, d->data, vector_size);

  qpointers->l2norm_data = new double[vector_size / (sizeof(double)*d->dim)];
  audiodb_l2norm_buffer(*vector_data, d->dim, nvectors, qpointers->l2norm_data);
  audiodb_sequence_sum(qpointers->l2norm_data, nvectors, sequence_length);
  audiodb_sequence_sqrt(qpointers->l2norm_data, nvectors, sequence_length);

  if(d->power) {
    qpointers->power_data = new double[vector_size / (sizeof(double)*d->dim)];
    memcpy(qpointers->power_data, d->power, vector_size / d->dim);
    audiodb_sequence_sum(qpointers->power_data, nvectors, sequence_length);
    audiodb_sequence_average(qpointers->power_data, nvectors, sequence_length);
  }

  if(d->times) {
    qpointers->mean_duration = new double[1];
    *qpointers->mean_duration = 0;
    for(unsigned int k = 0; k < nvectors; k++) {
      *qpointers->mean_duration += d->times[2*k+1] - d->times[2*k];
    }
    *qpointers->mean_duration /= nvectors;
  }

  *vector = *vector_data;
  qpointers->l2norm = qpointers->l2norm_data;
  qpointers->power = qpointers->power_data;
  return 0;
}

int audiodb_query_spec_qpointers(adb_t *adb, const adb_query_spec_t *spec, double **vector_data, double **vector, adb_qpointers_internal_t *qpointers) {
  adb_datum_t *datum;
  adb_datum_t d = {0};
  uint32_t sequence_length;
  uint32_t sequence_start;

  datum = spec->qid.datum;
  sequence_length = spec->qid.sequence_length;
  sequence_start = spec->qid.sequence_start;
    
  if(datum->data) {
    if(datum->dim != adb->header->dim) {
      return 1;
    }
    /* initialize d, and mark that nothing needs freeing later. */
    d = *datum;
    d.key = "";
    datum = &d;
  } else if (datum->key) {
    uint32_t track_id;
    if((track_id = audiodb_key_index(adb, datum->key)) == (uint32_t) -1) {
      return 1;
    }
    audiodb_track_id_datum(adb, track_id, &d);
  } else {
    return 1;
  }

  /* FIXME: check the overflow logic here */
  if(sequence_start + sequence_length > d.nvectors) {
    if(datum != &d) {
      audiodb_really_free_datum(&d);
    }
    return 1;
  }

  audiodb_datum_qpointers(&d, sequence_length, vector_data, vector, qpointers);

  /* Finally, if applicable, set up the moving qpointers. */
  if(spec->qid.flags & ADB_QID_FLAG_EXHAUSTIVE) {
    /* the qpointers are already at the start, and so correct. */
  } else {
    /* adjust the qpointers to point to the correct place in the sequence */
    *vector = *vector_data + spec->qid.sequence_start * d.dim;
    qpointers->l2norm = qpointers->l2norm_data + spec->qid.sequence_start;
    if(d.power) {
      qpointers->power = qpointers->power_data + spec->qid.sequence_start;
    }
    qpointers->nvectors = sequence_length;
  }

  /* Clean up: free any bits of datum that we have ourselves
   * allocated. */
  if(datum != &d) {
    audiodb_really_free_datum(&d);
  }

  return 0;
}

static int audiodb_set_up_dbpointers(adb_t *adb, const adb_query_spec_t *spec, adb_qpointers_internal_t *dbpointers) {
  uint32_t nvectors = adb->header->length / (adb->header->dim * sizeof(double));
  uint32_t sequence_length = spec->qid.sequence_length;

  bool using_power = spec->refine.flags & (ADB_REFINE_ABSOLUTE_THRESHOLD|ADB_REFINE_RELATIVE_THRESHOLD);
  bool using_times = spec->refine.flags & ADB_REFINE_DURATION_RATIO;
  double *times_table = NULL;


  dbpointers->nvectors = nvectors;
  dbpointers->l2norm_data = new double[nvectors];

  double *snpp = dbpointers->l2norm_data, *sppp = 0;
  lseek_set_or_goto_error(adb->fd, adb->header->l2normTableOffset);
  read_or_goto_error(adb->fd, dbpointers->l2norm_data, nvectors * sizeof(double));

  if (using_power) {
    if (!(adb->header->flags & ADB_HEADER_FLAG_POWER)) {
      goto error;
    }
    dbpointers->power_data = new double[nvectors];
    sppp = dbpointers->power_data;
    lseek_set_or_goto_error(adb->fd, adb->header->powerTableOffset);
    read_or_goto_error(adb->fd, dbpointers->power_data, nvectors * sizeof(double));
  }

  for(unsigned int i = 0; i < adb->header->numFiles; i++){
    size_t track_length = (*adb->track_lengths)[i];
    if(track_length >= sequence_length) {
      audiodb_sequence_sum(snpp, track_length, sequence_length);
      audiodb_sequence_sqrt(snpp, track_length, sequence_length);
      if (using_power) {
	audiodb_sequence_sum(sppp, track_length, sequence_length);
        audiodb_sequence_average(sppp, track_length, sequence_length);
      }
    }
    snpp += track_length;
    if (using_power) {
      sppp += track_length;
    }
  }

  if (using_times) {
    if(!(adb->header->flags & ADB_HEADER_FLAG_TIMES)) {
      goto error;
    }

    dbpointers->mean_duration = new double[adb->header->numFiles];

    malloc_and_fill_or_goto_error(double *, times_table, adb->header->timesTableOffset, 2 * nvectors * sizeof(double));
    for(unsigned int k = 0; k < adb->header->numFiles; k++) {
      size_t track_length = (*adb->track_lengths)[k];
      unsigned int j;
      dbpointers->mean_duration[k] = 0.0;
      for(j = 0; j < track_length; j++) {
	dbpointers->mean_duration[k] += times_table[2*j+1] - times_table[2*j];
      }
      dbpointers->mean_duration[k] /= j;
    }

    free(times_table);
    times_table = NULL;
  }

  dbpointers->l2norm = dbpointers->l2norm_data;
  dbpointers->power = dbpointers->power_data;
  return 0;

 error:
  maybe_delete_array(dbpointers->l2norm_data);
  maybe_delete_array(dbpointers->power_data);
  maybe_delete_array(dbpointers->mean_duration);
  maybe_free(times_table);
  return 1;
  
}

int audiodb_query_queue_loop(adb_t *adb, const adb_query_spec_t *spec, adb_qstate_internal_t *qstate, double *query, adb_qpointers_internal_t *qpointers) {
  adb_qpointers_internal_t dbpointers = {0};

  uint32_t sequence_length = spec->qid.sequence_length;
  bool power_refine = spec->refine.flags & (ADB_REFINE_ABSOLUTE_THRESHOLD|ADB_REFINE_RELATIVE_THRESHOLD);

  if(qstate->exact_evaluation_queue->size() == 0) {
    return 0;
  }

  /* We are guaranteed that the order of points is sorted by:
   * {trackID, spos, qpos} so we can be relatively efficient in
   * initialization of track data.  We assume that points usually
   * don't overlap, so we will use exhaustive dot product evaluation
   * (instead of memoization of partial sums, as in query_loop()).
   */
  double dist;
  double *dbdata = 0, *dbdata_pointer;
  uint32_t currentTrack = 0x80000000; // KLUDGE: Initialize with a value outside of track index range
  uint32_t npairs = qstate->exact_evaluation_queue->size();
  while(npairs--) {
    PointPair pp = qstate->exact_evaluation_queue->top();
    if(currentTrack != pp.trackID) {
      maybe_delete_array(dbdata);
      maybe_delete_array(dbpointers.l2norm_data);
      maybe_delete_array(dbpointers.power_data);
      maybe_delete_array(dbpointers.mean_duration);
      currentTrack = pp.trackID;
      adb_datum_t d = {0};
      if(audiodb_track_id_datum(adb, pp.trackID, &d)) {
        delete qstate->exact_evaluation_queue;
        return 1;
      }
      if(audiodb_datum_qpointers(&d, sequence_length, &dbdata, &dbdata_pointer, &dbpointers)) {
        delete qstate->exact_evaluation_queue;
        audiodb_really_free_datum(&d);
        return 1;
      }
      audiodb_really_free_datum(&d);
    }
    uint32_t qPos = (spec->qid.flags & ADB_QID_FLAG_EXHAUSTIVE) ? pp.qpos : 0;
    uint32_t sPos = pp.spos; // index into l2norm table
    // Test power thresholds before computing distance
    if( ( (!power_refine) || audiodb_powers_acceptable(&spec->refine, qpointers->power[qPos], dbpointers.power[sPos])) &&
	( qPos<qpointers->nvectors-sequence_length+1 && sPos<(*adb->track_lengths)[pp.trackID]-sequence_length+1 ) ){
      // Compute distance
      dist = audiodb_dot_product(query + qPos*adb->header->dim, dbdata + sPos*adb->header->dim, adb->header->dim*sequence_length);
      double qn = qpointers->l2norm[qPos];
      double sn = dbpointers.l2norm[sPos];
      switch(spec->params.distance) {
      case ADB_DISTANCE_EUCLIDEAN_NORMED:
	dist = 2 - (2/(qn*sn))*dist;
        break;
      case ADB_DISTANCE_EUCLIDEAN:
        dist = qn*qn + sn*sn - 2*dist;
        break;
      }
      if((!(spec->refine.flags & ADB_REFINE_RADIUS)) || 
         dist <= (spec->refine.radius + ADB_DISTANCE_TOLERANCE)) {
        adb_result_t r;
        r.ikey = (*adb->keys)[pp.trackID].c_str();
        r.qkey = spec->qid.datum->key;
        r.dist = dist;
        r.qpos = pp.qpos;
        r.ipos = pp.spos;
	if(qstate->set->find(r) == qstate->set->end()) {
	  qstate->set->insert(r);
	  qstate->accumulator->add_point(&r);
	}
      }
    }
    qstate->exact_evaluation_queue->pop();
  }

  // Cleanup
  maybe_delete_array(dbdata);
  maybe_delete_array(dbpointers.l2norm_data);
  maybe_delete_array(dbpointers.power_data);
  maybe_delete_array(dbpointers.mean_duration);
  delete qstate->exact_evaluation_queue;
  return 0;
}

int audiodb_query_loop(adb_t *adb, const adb_query_spec_t *spec, adb_qstate_internal_t *qstate) {
  
  double *query, *query_data;
  adb_qpointers_internal_t qpointers = {0}, dbpointers = {0};

  bool power_refine = spec->refine.flags & (ADB_REFINE_ABSOLUTE_THRESHOLD|ADB_REFINE_RELATIVE_THRESHOLD);

  if(adb->header->flags & ADB_HEADER_FLAG_REFERENCES) {
    /* FIXME: actually it would be nice to support this mode of
     * operation, but for now... */
    return 1;
  }

  if(audiodb_query_spec_qpointers(adb, spec, &query_data, &query, &qpointers)) {
    return 1;
  }

  if(audiodb_set_up_dbpointers(adb, spec, &dbpointers)) {
    return 1;
  }

  unsigned j,k,track,trackOffset=0;
  unsigned wL = spec->qid.sequence_length;
  double **D = 0;    // Differences query and target 
  double **DD = 0;   // Matched filter distance

  D = new double*[qpointers.nvectors]; // pre-allocate 
  DD = new double*[qpointers.nvectors];

  unsigned qhop, ihop;

  if(spec->refine.flags & ADB_REFINE_HOP_SIZE) {
    qhop = spec->refine.qhopsize;
    qhop = qhop ? qhop : 1;
    ihop = spec->refine.ihopsize;
    ihop = ihop ? ihop : 1;
  } else {
    qhop = 1;
    ihop = 1;
  }
  off_t trackIndexOffset;

  // Track loop 
  size_t data_buffer_size = 0;
  double *data_buffer = 0;
  lseek(adb->fd, adb->header->dataOffset, SEEK_SET);

  std::set<std::string>::iterator keys_end = qstate->allowed_keys->end();
  for(track = 0; track < adb->header->numFiles; track++) {
    unsigned t = track;
    
    while (qstate->allowed_keys->find((*adb->keys)[track]) == keys_end) {
      track++;
      if(track == adb->header->numFiles) {
        goto loop_finish;
      }
    }
    trackOffset = (*adb->track_offsets)[track];
    if(track != t) {
      lseek(adb->fd, adb->header->dataOffset + trackOffset, SEEK_SET);
    }
    trackIndexOffset = trackOffset / (adb->header->dim * sizeof(double)); // dbpointers.nvectors offset

    if(audiodb_read_data(adb, adb->fd, track, &data_buffer, &data_buffer_size)) {
      return 1;
    }
    if(wL <= (*adb->track_lengths)[track]) {  // test for short sequences
      
      audiodb_initialize_arrays(adb, spec, track, qpointers.nvectors, query, data_buffer, D, DD);

      if((!(spec->refine.flags & ADB_REFINE_DURATION_RATIO)) || 
         fabs(dbpointers.mean_duration[track]-qpointers.mean_duration[0]) < qpointers.mean_duration[0]*spec->refine.duration_ratio) {

	// Search for minimum distance by shingles (concatenated vectors)
	for(j = 0; j <= qpointers.nvectors - wL; j += qhop) {
	  for(k = 0; k <= (*adb->track_lengths)[track] - wL; k += ihop) {
            double thisDist = 0;
            double qn = qpointers.l2norm[j];
            double sn = dbpointers.l2norm[trackIndexOffset + k];
            switch(spec->params.distance) {
            case ADB_DISTANCE_EUCLIDEAN_NORMED:
              thisDist = 2-(2/(qn*sn))*DD[j][k];
              break;
            case ADB_DISTANCE_EUCLIDEAN:
              thisDist = qn*qn + sn*sn - 2*DD[j][k];
              break;
            case ADB_DISTANCE_DOT_PRODUCT:
              thisDist = DD[j][k];
              break;
            }
	    // Power test
	    if ((!power_refine) || audiodb_powers_acceptable(&spec->refine, qpointers.power[j], dbpointers.power[trackIndexOffset + k])) {
              // radius test
              if((!(spec->refine.flags & ADB_REFINE_RADIUS)) || 
                 thisDist <= (spec->refine.radius + ADB_DISTANCE_TOLERANCE)) {
                adb_result_t r;
                r.ikey = (*adb->keys)[track].c_str();
                r.qkey = spec->qid.datum->key;
                r.dist = thisDist;
                if(spec->qid.flags & ADB_QID_FLAG_EXHAUSTIVE) {
                  r.qpos = j;
                } else {
                  r.qpos = spec->qid.sequence_start;
                }
                r.ipos = k;
                qstate->accumulator->add_point(&r);
              }
            }
          }
        }
      } // Duration match            
      audiodb_delete_arrays(track, qpointers.nvectors, D, DD);
    }
  }

 loop_finish:

  free(data_buffer);
  maybe_delete_array(query_data);
  maybe_delete_array(qpointers.power_data);
  maybe_delete_array(qpointers.l2norm_data);
  maybe_delete_array(qpointers.mean_duration);
  maybe_delete_array(dbpointers.power_data);
  maybe_delete_array(dbpointers.l2norm_data);
  maybe_delete_array(dbpointers.mean_duration);
  maybe_delete_array(D);
  maybe_delete_array(DD);

  return 0;
}
