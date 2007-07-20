audioDB version 1.0

A feature vector database management system for content-based retrieval.

Usage: audioDB [OPTIONS]...

      --full-help              Print help, including hidden options, and exit
  -V, --version                Print version and exit
  -H, --help                   print help on audioDB usage and exit.
  -v, --verbosity=detail       level of detail of operational information.  
                                 (default=`1')

Database Setup:
  All database operations require a database argument.
  
  Database commands are UPPER CASE. Command options are lower case.

  -d, --database=filename      database file required by Database commands.
  -N, --NEW                    make a new (initially empty) database.
  -S, --STATUS                 output database information to stdout.
  -D, --DUMP                   output all entries: index key size.
  -L, --L2NORM                 unit norm vectors and norm all future inserts.

Database Insertion:
  The following commands insert feature files, with optional keys and 
  timestamps.

  -I, --INSERT                 add feature vectors to an existing database.
  -U, --UPDATE                 replace inserted vectors associated with key 
                                 with new input vectors.
  -f, --features=filename      binary series of vectors file {int sz:ieee 
                                 double[][sz]:eof}.
  -t, --times=filename         list of time points (ascii) for feature vectors.
  -k, --key=identifier         unique identifier associated with features.
  
  -B, --BATCHINSERT            add feature vectors named in a --featureList 
                                 file (with optional keys in a --keyList file) 
                                 to the named database.
  -F, --featureList=filename   text file containing list of binary feature 
                                 vector files to process
  -T, --timesList=filename     text file containing list of ascii --times for 
                                 each --features file in --featureList.
  -K, --keyList=filename       text file containing list of unique identifiers 
                                 associated with --features.

Database Search:
  Thse commands control the retrieval behaviour.

  -Q, --QUERY=searchtype       content-based search on --database using 
                                 --features as a query. Optionally restrict the 
                                 search to those segments identified in a 
                                 --keyList.  (possible values="point", 
                                 "segment", "sequence")
  -p, --qpoint=position        ordinal position of query start point in 
                                 --features file.  (default=`0')
  -e, --exhaustive             exhaustive search: iterate through all query 
                                 vectors in search. Overrides --qpoint.  
                                 (default=off)
  -n, --pointnn=numpoints      number of point nearest neighbours to use in 
                                 retrieval.  (default=`10')
  -R, --radius=DOUBLE          radius search, returns all 
                                 points/segments/sequences inside given radius. 
                                  (default=`1.0')
  -x, --expandfactor=DOUBLE    time compress/expand factor of result length to 
                                 query length [1.0 .. 100.0].  (default=`1.1')
  -o, --rotate                 rotate query vectors for rotationally invariant 
                                 search.  (default=off)
  -r, --resultlength=length    maximum length of the result list.  
                                 (default=`10')
  -l, --sequencelength=length  length of sequences for sequence search.  
                                 (default=`16')
  -h, --sequencehop=hop        hop size of sequence window for sequence search. 
                                  (default=`1')

Web Services:
  These commands enable the database process to establish a connection via the 
  internet and operate as separate client and server processes.

  -s, --SERVER=port            run as standalone web service on named port.  
                                 (default=`80011')
  -c, --client=hostname:port   run as a client using named host service.
  
  Copyright (C) 2007 Michael Casey, Goldsmiths, University of London

