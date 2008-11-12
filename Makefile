HELP2MAN=help2man
GENGETOPT=gengetopt
SOAPCPP2=soapcpp2
GSOAP_CPP=-lgsoap++
LIBGSL=-lgsl -lgslcblas
GSL_INCLUDE=
GSOAP_INCLUDE=

override CFLAGS+=-O3 -g -fPIC
#override CFLAGS+=-ggdb -gstabs+ -g3

ifeq ($(shell uname),Linux)
override CFLAGS+=-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
endif

ifeq ($(shell uname),Darwin)
ifeq ($(shell sysctl -n hw.optional.x86_64),1)
override CFLAGS+=-arch x86_64
endif
endif

LIBOBJS=insert.o create.o common.o dump.o query.o sample.o index.o lshlib.o
OBJS=$(LIBOBJS) soap.o


EXECUTABLE=audioDB
LIBRARY=libaudioDB_API.so


.PHONY: all clean test

all: $(OBJS) $(LIBRARY) $(EXECUTABLE) tags 

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


$(EXECUTABLE): cmdline.c $(OBJS) soapServer.cpp soapClient.cpp soapC.cpp
	g++ -c $(CFLAGS) $(GSOAP_INCLUDE) -Wall audioDB.cpp -DBINARY
	g++ -o $(EXECUTABLE) $(CFLAGS) audioDB.o $^ $(LIBGSL) $(GSOAP_INCLUDE) $(GSOAP_CPP)


$(LIBRARY): cmdline.c $(LIBOBJS)
	g++ -c $(CFLAGS) $(GSOAP_INCLUDE) -Wall audioDB.cpp
	g++ -shared -o $(LIBRARY) $(CFLAGS) $(LIBGSL) audioDB.o $^ 

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
	-rm $(LIBRARY)
	-rm tags

dist_clean:
	-rm cmdline.c cmdline.h
	-rm soapServer.cpp soapClient.cpp soapC.cpp soapObject.h soapStub.h soapProxy.h soapH.h soapServerLib.cpp soapClientLib.cpp
	-rm adb.*
	-rm HELP.txt
	-rm $(EXECUTABLE) $(EXECUTABLE).1 $(OBJS)
	-rm xthresh
	-sh -c "cd tests && sh ./clean.sh"
	-sh -c "cd libtests && sh ./clean.sh"
	-rm $(LIBRARY)
	-rm *.o
	-rm tags
	-rm -rf audioDB.dump


test: $(EXECUTABLE)
	-sh -c "cd tests && sh ./run-tests.sh"

xthresh: xthresh.c
	gcc -o $@ $(CFLAGS) $(GSL_INCLUDE) $(LIBGSL) $<

install:
	cp $(LIBRARY) /usr/local/lib/
	ldconfig
	cp audioDB_API.h /usr/local/include/

