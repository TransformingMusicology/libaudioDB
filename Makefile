HELP2MAN=help2man
GENGETOPT=gengetopt
SOAPCPP2=soapcpp2
GSOAP_CPP=-lgsoap++
LIBGSL=-lgsl -lgslcblas
GSL_INCLUDE=
GSOAP_INCLUDE=

override CFLAGS+=-O3 -g
#override CFLAGS+=-ggdb -gstabs+ -g3

ifeq ($(shell uname),Linux)
override CFLAGS+=-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
endif

ifeq ($(shell uname),Darwin)
ifeq ($(shell sysctl -n hw.optional.x86_64),1)
override CFLAGS+=-arch x86_64
endif
endif

EXECUTABLE=audioDB

.PHONY: all clean test

all: ${EXECUTABLE}

${EXECUTABLE}.1: ${EXECUTABLE}
	${HELP2MAN} ./${EXECUTABLE} > ${EXECUTABLE}.1

HELP.txt: ${EXECUTABLE}
	./${EXECUTABLE} --help > HELP.txt

cmdline.c cmdline.h: gengetopt.in
	${GENGETOPT} -e <gengetopt.in

soapServer.cpp soapClient.cpp soapC.cpp adb.nsmap: audioDBws.h
	${SOAPCPP2} audioDBws.h

%.o: %.cpp audioDB.h adb.nsmap cmdline.h reporter.h ReporterBase.h lshlib.h
	g++ -c ${CFLAGS} ${GSOAP_INCLUDE} ${GSL_INCLUDE} -Wall  $<

cmdline.o: cmdline.c cmdline.h
	gcc -c $<

OBJS=insert.o create.o common.o dump.o query.o soap.o sample.o audioDB.o index.o lshlib.o

${EXECUTABLE}: ${OBJS} soapServer.cpp soapClient.cpp soapC.cpp cmdline.o
	g++ -o ${EXECUTABLE} ${CFLAGS} ${GSL_INCLUDE} ${LIBGSL} ${GSOAP_INCLUDE} $^ ${GSOAP_CPP}

clean:
	-rm cmdline.c cmdline.h
	-rm soapServer.cpp soapClient.cpp soapC.cpp soapObject.h soapStub.h soapProxy.h soapH.h soapServerLib.cpp soapClientLib.cpp
	-rm adb.nsmap adb.xsd adb.wsdl adb.*.req.xml adb.*.res.xml
	-rm HELP.txt
	-rm ${EXECUTABLE} ${EXECUTABLE}.1 ${OBJS}
	-rm xthresh
	-sh -c "cd tests && sh ./clean.sh"

test: ${EXECUTABLE}
	-sh -c "cd tests && sh ./run-tests.sh"

xthresh: xthresh.c
	gcc -o $@ ${CFLAGS} ${GSL_INCLUDE} ${LIBGSL} $<
