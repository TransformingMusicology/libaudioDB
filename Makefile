HELP2MAN=help2man
GENGETOPT=gengetopt
SOAPCPP2=soapcpp2
GSOAP_CPP=-lgsoap++
LIBGSL=-lgsl -lgslcblas
GSL_INCLUDE=
GSOAP_INCLUDE=

override CFLAGS+=-O3 -g -fPIC

ifeq ($(shell uname),Linux)
override CFLAGS+=-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
endif

ifeq ($(shell uname),Darwin)
ifeq ($(shell sysctl -n hw.optional.x86_64),1)
override CFLAGS+=-arch x86_64
endif
endif

LIBOBJS=insert.o create.o common.o dump.o query.o sample.o index.o lshlib.o cmdline.o
OBJS=$(LIBOBJS) soap.o audioDB.o


EXECUTABLE=audioDB
LIBRARY=libaudioDB_API.so


.PHONY: all clean test

all: $(LIBRARY) $(EXECUTABLE)

$(EXECUTABLE).1: $(EXECUTABLE)
	$(HELP2MAN) ./$(EXECUTABLE) > $(EXECUTABLE).1

HELP.txt: $(EXECUTABLE)
	./$(EXECUTABLE) --help > HELP.txt

cmdline.c cmdline.h: gengetopt.in
	$(GENGETOPT) -e <gengetopt.in

soapServer.cpp soapClient.cpp soapC.cpp adb.nsmap: audioDBws.h
	$(SOAPCPP2) audioDBws.h

%.o: %.cpp audioDB.h adb.nsmap cmdline.h reporter.h ReporterBase.h lshlib.h
	g++ -c $(CFLAGS) $(GSOAP_INCLUDE) $(GSL_INCLUDE) -Wall  $<

cmdline.o: cmdline.c cmdline.h
	gcc -c $(CFLAGS) $<

audioDB_library.o: audioDB.cpp
	g++ -c -o $@ $(CFLAGS) $(GSOAP_INCLUDE) -Wall -DLIBRARY $< 

$(EXECUTABLE): $(OBJS) soapServer.cpp soapClient.cpp soapC.cpp
	g++ -o $(EXECUTABLE) $(CFLAGS) $^ $(LIBGSL) $(GSOAP_INCLUDE) $(GSOAP_CPP)


$(LIBRARY): $(LIBOBJS) audioDB_library.o
	g++ -shared -o $(LIBRARY) $(CFLAGS) $(LIBGSL) $^ 

tags:
	ctags *.cpp *.h


clean:
	-rm cmdline.c cmdline.h
	-rm soapServer.cpp soapClient.cpp soapC.cpp soapObject.h soapStub.h soapProxy.h soapH.h soapServerLib.cpp soapClientLib.cpp
	-rm adb.*
	-rm HELP.txt
	-rm $(EXECUTABLE) $(EXECUTABLE).1 $(OBJS)
	-rm xthresh
	-sh -c "cd tests && sh ./clean.sh"
	-sh -c "cd libtests && sh ./clean.sh"
	-rm $(LIBRARY) audioDB_library.o
	-rm tags

distclean: clean
	-rm *.o
	-rm -rf audioDB.dump


test: $(EXECUTABLE)
	-sh -c "cd tests && sh ./run-tests.sh"

xthresh: xthresh.c
	gcc -o $@ $(CFLAGS) $(GSL_INCLUDE) $(LIBGSL) $<

install:
	cp $(LIBRARY) /usr/local/lib/
	ldconfig
	cp audioDB_API.h /usr/local/include/

