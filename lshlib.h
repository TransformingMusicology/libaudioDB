// lshlib.h - a library for locality sensitive hashtable insertion and retrieval
//
// Author: Michael Casey
// Copyright (c) 2008 Michael Casey, All Rights Reserved

/*  		    GNU GENERAL PUBLIC LICENSE
		       Version 2, June 1991
		       See LICENSE.txt
*/

#ifndef __LSHLIB_H
#define __LSHLIB_H

#include <vector>
#include <queue>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <math.h>
#include <sys/time.h>
#include <assert.h>
#include <float.h>
#include <signal.h>
#include <time.h>
#include <limits.h>
#include <errno.h>
#ifdef MT19937
#include "mt19937/mt19937ar.h"
#endif

#define IntT int
#define LongUns64T long long unsigned
#define Uns32T unsigned
#define Int32T int
#define BooleanT int
#define TRUE 1
#define FALSE 0

// A big number (>> max #  of points)
#define INDEX_START_EMPTY 1000000000U

// 4294967291 = 2^32-5
#define UH_PRIME_DEFAULT 4294967291U

// 2^29
#define MAX_HASH_RND 536870912U

// 2^32-1
#define TWO_TO_32_MINUS_1 4294967295U

#define O2_SERIAL_VERSION 1 // Sync with SVN version
#define O2_SERIAL_HEADER_SIZE sizeof(SerialHeaderT)
#define O2_SERIAL_ELEMENT_SIZE sizeof(SerialElementT)
#define O2_SERIAL_MAX_TABLES (200)
#define O2_SERIAL_MAX_ROWS (1000000)
#define O2_SERIAL_MAX_COLS (1000)
#define O2_SERIAL_MAX_DIM (2000)
#define O2_SERIAL_MAX_FUNS (100)
#define O2_SERIAL_MAX_BINWIDTH (200)

// Flags for Serial Header
#define O2_SERIAL_FILEFORMAT1 (0x1U)       // Optimize for on-disk search
#define O2_SERIAL_FILEFORMAT2 (0x2U)       // Optimize for in-core search

// Flags for serialization fileformat2: use high 3 bits of Uns32T
#define O2_SERIAL_FLAGS_T1_BIT (0x80000000U)
#define O2_SERIAL_FLAGS_T2_BIT (0x40000000U)
#define O2_SERIAL_FLAGS_END_BIT (0x20000000U)

unsigned align_up(unsigned x, unsigned w);

#define O2_SERIAL_FUNCTIONS_SIZE (align_up(sizeof(float) * O2_SERIAL_MAX_TABLES * O2_SERIAL_MAX_FUNS * O2_SERIAL_MAX_DIM \
+ sizeof(float) * O2_SERIAL_MAX_TABLES * O2_SERIAL_MAX_FUNS + \
+ sizeof(Uns32T) * O2_SERIAL_MAX_TABLES * O2_SERIAL_MAX_FUNS * 2 \
+ O2_SERIAL_HEADER_SIZE,get_page_logn()))

#define O2_SERIAL_MAX_LSH_SIZE (O2_SERIAL_ELEMENT_SIZE * O2_SERIAL_MAX_TABLES \
			    * O2_SERIAL_MAX_ROWS * O2_SERIAL_MAX_COLS + O2_SERIAL_FUNCTIONS_SIZE)

#define O2_SERIAL_MAGIC ('o'|'2'<<8|'l'<<16|'s'<<24)

using namespace std;

Uns32T get_page_logn();

// Disk table entry
typedef class SerialElement SerialElementT;
class SerialElement {
public:
  Uns32T hashValue;
  Uns32T pointID;
  
  SerialElement(Uns32T h, Uns32T pID): 
    hashValue(h), 
    pointID(pID){}
};

// Disk header
typedef class SerialHeader SerialHeaderT;
class SerialHeader {
public:
  Uns32T lshMagic;    // unique identifier for file header
  float  binWidth;    // hash-function bin width
  Uns32T numTables;   // number of hash tables in file
  Uns32T numRows;     // size of each hash table
  Uns32T numCols;     // max collisions in each hash table
  Uns32T elementSize; // size of a hash bucket
  Uns32T version;     // version number of file format
  Uns32T size;        // total size of database (bytes)
  Uns32T flags;       // 32 bits of useful information
  Uns32T dataDim;     // vector dimensionality
  Uns32T numFuns;     // number of independent hash functions
  float radius;       // 32-bit floating point radius
  Uns32T maxp;        // number of unique IDs in the database
  Uns32T value_14;    // spare value
  Uns32T value_15;    // spare value
  Uns32T value_16;    // spare value

  SerialHeader();
  SerialHeader(float W, Uns32T L, Uns32T N, Uns32T C, Uns32T k, Uns32T d, float radius, Uns32T p, Uns32T FMT);

  float get_binWidth(){return binWidth;}  
  Uns32T get_numTables(){return numTables;}
  Uns32T get_numRows(){return numRows;}
  Uns32T get_numCols(){return numCols;}
  Uns32T get_elementSize(){return elementSize;}
  Uns32T get_version(){return version;}
  Uns32T get_flags(){return flags;}
  Uns32T get_size(){return size;}
  Uns32T get_dataDim(){return dataDim;}
  Uns32T get_numFuns(){return numFuns;}
  Uns32T get_maxp(){return maxp;}
};

#define IFLAG 0xFFFFFFFF

// Point-set collision bucket (sbucket). 
// sbuckets form a collision chain that identifies PointIDs falling in the same locale.
// sbuckets are chained from a bucket containing the collision list's t2 identifier
class sbucket {
  friend class bucket;
  friend class H;
  friend class G;

 public:
  class sbucket* snext;
  unsigned int pointID;

  sbucket(){
    snext=0;
    pointID=IFLAG;
  }
  ~sbucket(){delete snext;}
  sbucket* get_snext(){return snext;}
};

// bucket structure for a linked list of locales that collide with the same hash value t1
// different buckets represent different locales, collisions within a locale are chained
// in sbuckets
class bucket {
  friend class H;
  friend class G;
  bucket* next;
  sbucket* snext;
 public:
  unsigned int t2;  
  bucket(){
    next=0;
    snext=0;
    t2=IFLAG;
  }
  ~bucket(){delete next;delete snext;}
  bucket* get_next(){return next;}
};


// The hash_functions for locality-sensitive hashing
class H{
  friend class G;
 private:
  bucket*** h;      // hash functions
  Uns32T** r1;     // random ints for hashing
  Uns32T** r2;     // random ints for hashing
  Uns32T t1;       // first hash table key
  Uns32T t2;       // second hash table key
  Uns32T P;        // hash table prime number
  bool use_u_functions; // flag to optimize computation of hashes
  Uns32T bucketCount;  // count of number of point buckets allocated
  Uns32T pointCount;    // count of number of points inserted

  void __initialize_data_structures();
  void __bucket_insert_point(bucket*);
  void __sbucket_insert_point(sbucket*);
  Uns32T __computeProductModDefaultPrime(Uns32T*,Uns32T*,IntT);
  Uns32T __randr();
  bucket** __get_bucket(int j);
  void __generate_hash_keys(Uns32T*,Uns32T*,Uns32T*);
  void error(const char* a, const char* b = "", const char *sysFunc = 0);

 public:
  Uns32T N; // num rows per table
  Uns32T C; // num collision per row
  Uns32T k; // num projections per hash function
  Uns32T m; // ~sqrt num hash tables
  Uns32T L; // L = m*(m-1)/2, conversely, m = (1 + sqrt(1 + 8.0*L)) / 2.0
  Uns32T d; // dimensions
  Uns32T p; // current point

  H(){;}
  H(Uns32T k, Uns32T m, Uns32T d, Uns32T N, Uns32T C);
  ~H();


  Uns32T bucket_insert_point(bucket**);

  Uns32T get_t1(){return t1;}
  Uns32T get_t2(){return t2;}

};


typedef void (*ReporterCallbackPtr)(void* objPtr, Uns32T pointID, Uns32T queryIndex, float squaredDistance);

// Interface for indexing and retrieval
class G: public H{
 private:
  float *** A; // m x k x d random projectors from R^d->R^k
  float ** b;  // m x k uniform additive constants
  Uns32T ** g; // L x k random hash projections \in Z^k    
  float w;     // width of hash slots (relative to normalized feature space)
  float radius;// scaling coefficient for data (1./radius)
  vector<vector<Uns32T> > uu; // Storage for m patial hash evaluations
  Uns32T maxp; // highest pointID stored in database
  void* calling_instance; // store calling object instance for member-function callback
  void (*add_point_callback)(void*, Uns32T, Uns32T, float);

  void initialize_partial_functions();

  void get_lock(int fd, bool exclusive);
  void release_lock(int fd);
  int serial_create(char* filename, Uns32T FMT);
  int serial_create(char* filename, float binWidth, Uns32T nTables, Uns32T nRows, Uns32T nCols, Uns32T k, Uns32T d, Uns32T FMT);
  char* serial_mmap(int dbfid, Uns32T sz, Uns32T w, off_t offset = 0);
  void serial_munmap(char* db, Uns32T N);
  int serial_open(char* filename,int writeFlag);
  void serial_close(int dbfid);

  // Function to write hashfunctions to disk
  int serialize_lsh_hashfunctions(int fid);

  // Functions to write hashtables to disk in format1 (optimized for on-disk retrieval)
  int serialize_lsh_hashtables_format1(int fid, int merge);
  void serial_write_hashtable_row_format1(SerialElementT*& pe, bucket* h, Uns32T& colCount);
  void serial_write_element_format1(SerialElementT*& pe, sbucket* sb, Uns32T t2, Uns32T& colCount);
  void serial_merge_hashtable_row_format1(SerialElementT* pr, bucket* h, Uns32T& colCount);
  void serial_merge_element_format1(SerialElementT* pe, sbucket* sb, Uns32T t2, Uns32T& colCount);
  int serial_can_merge(Uns32T requestedFormat); // Test to see whether core and on-disk structures are compatible

  // Functions to write hashtables to disk in format2 (optimized for in-core retrieval)
  int serialize_lsh_hashtables_format2(int fid, int merge);
  void serial_write_hashtable_row_format2(int fid, bucket* h, Uns32T& colCount);
  void serial_write_element_format2(int fid, sbucket* sb, Uns32T& colCount);

  // Functions to read serial header and hash functions (format1 and format2)
  int unserialize_lsh_header(char* filename);            // read lsh header from disk into core
  void unserialize_lsh_functions(int fid);               // read the lsh hash functions into core

  // Functions to read hashtables in format1
  void unserialize_lsh_hashtables_format1(int fid);       // read FORMAT1 hash tables into core (disk format)
  void unserialize_hashtable_row_format1(SerialElementT* pe, bucket** b); // read lsh hash table row into core

  // Functions to read hashtables in format2
  void unserialize_lsh_hashtables_format2(int fid);       // read FORMAT2 hash tables into core (core format)
  Uns32T unserialize_hashtable_row_format2(int fid, bucket** b); // read lsh hash table row into core

  // Helper functions
  void serial_print_header(Uns32T requestedFormat);
  float* get_serial_hashfunction_base(char* db);
  SerialElementT* get_serial_hashtable_base(char* db);  
  Uns32T get_serial_hashtable_offset();                   // Size of SerialHeader + HashFunctions
  SerialHeaderT* serial_get_header(char* db);
  SerialHeaderT* lshHeader;

  // Core Retrieval/Inspections Functions
  void bucket_chain_point(bucket* p, Uns32T qpos);
  void sbucket_chain_point(sbucket* p, Uns32T qpos);  
  void dump_hashtable_row(bucket* p);

  // Serial (Format 1) Retrieval/Inspection Functions
  void serial_bucket_chain_point(SerialElementT* pe, Uns32T qpos);
  void serial_bucket_dump(SerialElementT* pe);

  // Hash functions
  void compute_hash_functions(vector<float>& v);
  float randn();
  float ranf();

  char* db;    // pointer to serialized structure

 public:
  G(char* lshFile, bool lshInCore = false); // unserialize constructor
  G(float w, Uns32T k,Uns32T m, Uns32T d, Uns32T N, Uns32T C, float r); // core constructor
  ~G();

  Uns32T insert_point(vector<float>&, Uns32T pointID);
  void insert_point_set(vector<vector<float> >& vv, Uns32T basePointID);

  // point retrieval from core
  void retrieve_point(vector<float>& v, Uns32T qpos, ReporterCallbackPtr, void* me=NULL);
  // point set retrieval from core
  void retrieve_point_set(vector<vector<float> >& vv, ReporterCallbackPtr, void* me=NULL);
  // serial point set retrieval
  void serial_retrieve_point_set(char* filename, vector<vector<float> >& vv, ReporterCallbackPtr, void* me=NULL);
  // serial point retrieval
  void serial_retrieve_point(char* filename, vector<float>& vv, Uns32T qpos, ReporterCallbackPtr, void* me=NULL);

  void serialize(char* filename, Uns32T serialFormat = O2_SERIAL_FILEFORMAT1); // write hashfunctions and hashtables to disk

  SerialHeaderT* get_lshHeader(){return lshHeader;}
  float get_radius(){return radius;}  
  Uns32T get_maxp(){return maxp;}
  void serial_dump_tables(char* filename);
  float get_mean_collision_rate(){ return (float) pointCount / bucketCount ; }
};

typedef class G LSH;

 

#endif
