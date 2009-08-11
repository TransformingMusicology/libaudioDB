extern "C" {
#include "audioDB_API.h"
}
#include "audioDB-internals.h"
#include "lshlib.h"

/*
 * Routines which are common to both indexed query and index creation:
 * we put them in their own file for build logistics.
 */

/* FIXME: there are several things wrong with this: the memory
 * discipline isn't ideal, the radius printing is a bit lame, the name
 * getting will succeed or fail depending on whether the path was
 * relative or absolute -- but most importantly encoding all that
 * information in a filename is going to lose: it's impossible to
 * maintain backwards-compatibility.  Instead we should probably store
 * the index metadata inside the audiodb instance. */
char *audiodb_index_get_name(const char *dbName, double radius, uint32_t sequenceLength) {
  char *indexName;
  if(strlen(dbName) > (ADB_MAXSTR - 32)) {
    return NULL;
  }
  indexName = new char[ADB_MAXSTR];  
  strncpy(indexName, dbName, ADB_MAXSTR);
  sprintf(indexName+strlen(dbName), ".lsh.%019.9f.%d", radius, sequenceLength);
  return indexName;
}

bool audiodb_index_exists(const char *dbName, double radius, uint32_t sequenceLength) {
  char *indexName = audiodb_index_get_name(dbName, radius, sequenceLength);
  if(!indexName) {
    return false;
  }
  struct stat st;
  if(stat(indexName, &st)) {
    delete [] indexName;
    return false;
  }
  /* FIXME: other stat checks here? */
  /* FIXME: is there any better way to check whether we can open a
   * file for reading than by opening a file for reading? */
  int fd = open(indexName, O_RDONLY);
  delete [] indexName;
  if(fd < 0) {
    return false;
  } else {
    close(fd);
    return true;
  }
}

/* FIXME: the indexName arg should be "const char *", but the LSH
 * library doesn't like that.
 */
LSH *audiodb_index_allocate(adb_t *adb, char *indexName, bool load_tables) {
  LSH *lsh;
  if(adb->cached_lsh) {
    if(!strncmp(adb->cached_lsh->get_indexName(), indexName, ADB_MAXSTR)) {
      return adb->cached_lsh;
    } else {
      delete adb->cached_lsh;
    }
  }
  lsh = new LSH(indexName, load_tables);
  if(load_tables) {
    adb->cached_lsh = lsh;
  } 
  return lsh;
}

vector<vector<float> > *audiodb_index_initialize_shingles(uint32_t sz, uint32_t dim, uint32_t seqLen) {
  std::vector<std::vector<float> > *vv = new vector<vector<float> >(sz);
  for(uint32_t i=0 ; i < sz ; i++) {
    (*vv)[i]=vector<float>(dim * seqLen);
  }
  return vv;
}

void audiodb_index_delete_shingles(vector<vector<float> > *vv) {
  delete vv;
}

void audiodb_index_make_shingle(vector<vector<float> >* vv, uint32_t idx, double* fvp, uint32_t dim, uint32_t seqLen){

  vector<float>::iterator ve = (*vv)[idx].end();
  vector<float>::iterator vi = (*vv)[idx].begin();
  // First feature vector in shingle
  if(idx == 0) {
    while(vi!=ve) {
      *vi++ = (float)(*fvp++);
    }
  } else {
    // Not first feature vector in shingle
    vector<float>::iterator ui=(*vv)[idx-1].begin() + dim;
    // Previous seqLen-1 dim-vectors
    while(vi!=ve-dim) {
      *vi++ = *ui++;
    }
    // Move data pointer to next feature vector
    fvp += ( seqLen + idx - 1 ) * dim ;
    // New d-vector
    while(vi!=ve) {
      *vi++ = (float)(*fvp++);
    }
  }
}

// in-place norming, no deletions.  If using power, return number of
// shingles above power threshold.
int audiodb_index_norm_shingles(vector<vector<float> >* vv, double* snp, double* spp, uint32_t dim, uint32_t seqLen, double radius, bool normed_vectors, bool use_pthreshold, float pthreshold) {
  int z = 0; // number of above-threshold shingles
  float l2norm;
  double power;
  float oneOverRadius = 1./(float)sqrt(radius); // Passed radius is really radius^2
  float oneOverSqrtl2NormDivRad = oneOverRadius;
  uint32_t shingleSize = seqLen * dim;

  if(!spp) {
    return -1;
  }
  for(uint32_t a=0; a<(*vv).size(); a++){
    l2norm = (float)(*snp++);
    if(normed_vectors)
      oneOverSqrtl2NormDivRad = (1./l2norm)*oneOverRadius;
    
    for(uint32_t b=0; b < shingleSize ; b++)
      (*vv)[a][b]*=oneOverSqrtl2NormDivRad;

    power = *spp++;
    if(use_pthreshold){
      if (power >= pthreshold)
	z++;
    }
    else
      z++;	
  }
  return z;
}
