HELP2MAN=help2man
GENGETOPT=gengetopt
SOAPCPP2=soapcpp2
GSOAP_CPP=-lgsoap++
LIBGSL=-lgsl -lgslcblas
GSL_INCLUDE=
GSOAP_INCLUDE=

SHARED_LIB_FLAGS=-shared -Wl,-soname,

LIBOBJS=query.o index.o insert.o create.o common.o open.o close.o status.o dump.o power.o l2norm.o lshlib.o lock.o pointpair.o
OBJS=$(LIBOBJS) soap.o liszt.o sample.o cmdline.o audioDB.o common.o


EXECUTABLE=audioDB

SOVERSION=0
MINORVERSION=0
LIBRARY=lib$(EXECUTABLE).so.$(SOVERSION).$(MINORVERSION)

override CFLAGS+=-O3 -g -fPIC

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

$(LIBOBJS): %.o: %.cpp audioDB.h audioDB_API.h audioDB-internals.h accumulator.h accumulators.h cmdline.h soapH.h
	g++ -c $(CFLAGS) $(GSL_INCLUDE) -Wall $<

%.o: %.cpp audioDB.h audioDB_API.h adb.nsmap cmdline.h reporter.h ReporterBase.h lshlib.h
	g++ -c $(CFLAGS) $(GSOAP_INCLUDE) $(GSL_INCLUDE) -Wall  $<

cmdline.o: cmdline.c cmdline.h
	gcc -c $(CFLAGS) $<

$(EXECUTABLE): $(OBJS) soapServer.cpp soapClient.cpp soapC.cpp
	g++ -o $(EXECUTABLE) $(CFLAGS) $^ $(LIBGSL) $(GSOAP_INCLUDE) $(GSOAP_CPP)

$(LIBRARY): $(LIBOBJS)
	g++ $(SHARED_LIB_FLAGS)$(LIBRARY) -o $(LIBRARY) $(CFLAGS) $(LIBGSL) $^ 

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


test: $(EXECUTABLE)
	-sh -c "cd tests && sh ./run-tests.sh"
	-sh -c "cd libtests && sh ./run-tests.sh"

xthresh: xthresh.c
	gcc -o $@ $(CFLAGS) $(GSL_INCLUDE) $(LIBGSL) $<

install:
	cp $(LIBRARY) /usr/local/lib/
	ln -sf /usr/local/lib/$(LIBRARY) /usr/local/lib/lib$(EXECUTABLE).so.$(SOVERSION)
	ln -sf /usr/local/lib/lib$(EXECUTABLE).so.$(SOVERSION) /usr/local/lib/lib$(EXECUTABLE).so
	ldconfig
	cp audioDB_API.h /usr/local/include/
