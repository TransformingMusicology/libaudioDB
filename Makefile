GSL_INCLUDE=$(shell pkg-config --cflags gsl)
LIBGSL=$(shell pkg-config --libs gsl)

TESTDIRS=libtests

PREFIX=/usr/local
LIBDIR=$(PREFIX)/lib
INCLUDEDIR=$(PREFIX)/include

LIBOBJS=lock.o pointpair.o create.o open.o power.o l2norm.o insert.o status.o query.o dump.o close.o index-utils.o query-indexed.o liszt.o retrieve.o lshlib.o multiprobe.o sample.o

LIBNAME=audioDB

SOVERSION=0
MINORVERSION=0
LIBRARY=lib$(LIBNAME).so.$(SOVERSION).$(MINORVERSION)
SHARED_LIB_FLAGS=-shared -Wl,-soname,lib$(LIBNAME).so.$(SOVERSION)

override CFLAGS+=-g -O3 -fPIC 

# set to generate profile (gprof) and coverage (gcov) info
#override CFLAGS+=-fprofile-arcs -ftest-coverage -pg

# set to DUMP hashtables on QUERY load
#override CFLAGS+=-DLSH_DUMP_CORE_TABLES

ifeq ($(shell uname),Linux)
override CFLAGS+=-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
endif

ifeq ($(shell uname),Darwin)
ifeq ($(shell sysctl -n hw.optional.x86_64),1)
override CFLAGS+=-arch x86_64
endif
override LIBRARY=lib$(LIBNAME).$(SOVERSION).$(MINORVERSION).dylib
override SHARED_LIB_FLAGS=-dynamiclib -current_version $(SOVERSION) -Wl -install_name $(LIBRARY)
endif

.PHONY: all clean test install 

all: $(LIBRARY)

$(LIBOBJS): %.o: %.cpp audioDB_API.h audioDB-internals.h accumulator.h accumulators.h
	$(CXX) -c $(CFLAGS) $(GSL_INCLUDE) -Wall $<

%.o: %.cpp audioDB.h audioDB_API.h adb.nsmap reporter.h ReporterBase.h lshlib.h
	$(CXX) -c $(CFLAGS) $(GSL_INCLUDE) -Wall  $<

$(LIBRARY): $(LIBOBJS)
	$(CXX) $(SHARED_LIB_FLAGS) -o $(LIBRARY) $(LIBGSL) $(CFLAGS) $^

tags:
	ctags *.cpp *.h

testclean: 
	-for d in $(TESTDIRS); do (cd $$d && sh ./clean.sh); done

clean: testclean
	-rm adb.*
	-rm $(LIBOBJS)
	-rm xthresh
	-rm $(LIBRARY)
	-rm tags
	-rm $(LIBNAME).pc

distclean: clean
	-rm *.o
	-rm -rf audioDB.dump

test: $(LIBRARY)
	for d in $(TESTDIRS); do (cd $$d && sh ./run-tests.sh); done

xthresh: xthresh.c
	$(CC) -o $@ $(CFLAGS) $(GSL_INCLUDE) $(LIBGSL) $<

$(LIBNAME).pc:
	./make-pc.sh "$(LIBNAME)" "$(LIBDIR)" "$(INCLUDEDIR)" $(SOVERSION) $(MINORVERSION) > $(LIBNAME).pc

install: $(LIBNAME).pc
	mkdir -m755 -p $(LIBDIR)/pkgconfig $(INCLUDEDIR)
	install -m644 $(LIBRARY) $(LIBDIR)
ifneq ($(shell uname),Darwin)
	ldconfig -n $(LIBDIR)
endif
	ln -sf $(LIBRARY) $(LIBDIR)/lib$(LIBNAME).so
	install -m644 audioDB_API.h $(INCLUDEDIR)
	install $(LIBNAME).pc $(LIBDIR)/pkgconfig
