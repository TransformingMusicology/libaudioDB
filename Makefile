HELP2MAN=help2man
GENGETOPT=gengetopt
SOAPCPP2=soapcpp2
GSOAP_CPP=-lgsoap++
LIBGSL=-lgsl -lgslcblas
GSL_INCLUDE=
GSOAP_INCLUDE=

SHARED_LIB_FLAGS=-shared -Wl,-soname,

LIBOBJS=lock.o pointpair.o create.o open.o power.o l2norm.o insert.o status.o query.o dump.o close.o index-utils.o query-indexed.o liszt.o retrieve.o lshlib.o
OBJS=$(LIBOBJS) index.o soap.o sample.o cmdline.o audioDB.o common.o

EXECUTABLE=audioDB

SOVERSION=0
MINORVERSION=0
LIBRARY=lib$(EXECUTABLE).so.$(SOVERSION).$(MINORVERSION)

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
override SHARED_LIB_FLAGS=-dynamiclib -current_version $(SOVERSION).$(MINORVERSION) -Wl -install_name 
override LIBRARY=lib$(EXECUTABLE).$(SOVERSION).$(MINORVERSION).dylib
endif

.PHONY: all clean test

all: $(LIBRARY) $(EXECUTABLE)

$(EXECUTABLE).1: $(EXECUTABLE)
	$(HELP2MAN) ./$(EXECUTABLE) > $(EXECUTABLE).1

HELP.txt: $(EXECUTABLE)
	./$(EXECUTABLE) --help > HELP.txt

cmdline.c cmdline.h: gengetopt.in
	$(GENGETOPT) -e <gengetopt.in

soapServer.cpp soapClient.cpp soapC.cpp soapH.h adb.nsmap: audioDBws.h
	$(SOAPCPP2) audioDBws.h

$(LIBOBJS): %.o: %.cpp audioDB_API.h audioDB-internals.h accumulator.h accumulators.h
	$(CXX) -c $(CFLAGS) $(GSL_INCLUDE) -Wall $<

%.o: %.cpp audioDB.h audioDB_API.h adb.nsmap cmdline.h reporter.h ReporterBase.h lshlib.h
	$(CXX) -c $(CFLAGS) $(GSOAP_INCLUDE) $(GSL_INCLUDE) -Wall  $<

cmdline.o: cmdline.c cmdline.h
	$(CC) -c $(CFLAGS) $<

$(EXECUTABLE): $(OBJS) soapServer.cpp soapClient.cpp soapC.cpp
	$(CXX) -o $(EXECUTABLE) $(CFLAGS) $^ $(LIBGSL) $(GSOAP_INCLUDE) $(GSOAP_CPP)

$(LIBRARY): $(LIBOBJS)
	$(CXX) $(SHARED_LIB_FLAGS)$(LIBRARY) -o $(LIBRARY) $(CFLAGS) $^

tags:
	ctags *.cpp *.h


clean:
	-rm cmdline.c cmdline.h cmdline.o
	-rm soapServer.cpp soapClient.cpp soapC.cpp soapObject.h soapStub.h soapProxy.h soapH.h soapServerLib.cpp soapClientLib.cpp
	-rm adb.*
	-rm HELP.txt
	-rm $(EXECUTABLE) $(EXECUTABLE).1 $(OBJS)
	-rm xthresh
	-sh -c "cd tests && sh ./clean.sh"
	-sh -c "cd libtests && sh ./clean.sh"
	-rm $(LIBRARY)
	-rm tags

distclean: clean
	-rm *.o
	-rm -rf audioDB.dump


test: $(EXECUTABLE) $(LIBRARY)
	sh -c "cd libtests && sh ./run-tests.sh"
	sh -c "cd tests && sh ./run-tests.sh"

xthresh: xthresh.c
	$(CC) -o $@ $(CFLAGS) $(GSL_INCLUDE) $(LIBGSL) $<

install:
	cp $(LIBRARY) /usr/local/lib/
	ln -sf /usr/local/lib/$(LIBRARY) /usr/local/lib/lib$(EXECUTABLE).so.$(SOVERSION)
	ln -sf /usr/local/lib/lib$(EXECUTABLE).so.$(SOVERSION) /usr/local/lib/lib$(EXECUTABLE).so
	ldconfig
	cp audioDB_API.h /usr/local/include/
