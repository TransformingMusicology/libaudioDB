HELP2MAN=help2man
GENGETOPT=gengetopt
SOAPCPP2=soapcpp2
GSOAP_CPP=-lgsoap++
GSOAP_INCLUDE=

CFLAGS=-O3 -g

EXECUTABLE=audioDB

.PHONY: all clean test

all: ${EXECUTABLE}

${EXECUTABLE}.1: ${EXECUTABLE}
	${HELP2MAN} ./${EXECUTABLE} > ${EXECUTABLE}.1

HELP.txt: ${EXECUTABLE}
	./${EXECUTABLE} --help > HELP.txt

cmdline.c cmdline.h: gengetopt.in
	${GENGETOPT} -e <gengetopt.in

soapServer.cpp soapClient.cpp soapC.cpp: audioDBws.h
	${SOAPCPP2} audioDBws.h

${EXECUTABLE}: audioDB.cpp audioDB.h soapServer.cpp soapClient.cpp soapC.cpp cmdline.c cmdline.h
	g++ -c ${CFLAGS} ${GSOAP_INCLUDE} -Wall -Werror audioDB.cpp
	g++ -o ${EXECUTABLE} ${CFLAGS} ${GSOAP_INCLUDE} audioDB.o soapServer.cpp soapClient.cpp soapC.cpp cmdline.c ${GSOAP_CPP}

clean:
	-rm cmdline.c cmdline.h
	-rm soapServer.cpp soapClient.cpp soapC.cpp soapObject.h soapStub.h soapProxy.h soapH.h soapServerLib.cpp soapClientLib.cpp
	-rm adb.nsmap adb.xsd adb.wsdl adb.query.req.xml adb.query.res.xml adb.status.req.xml adb.status.res.xml
	-rm HELP.txt
	-rm ${EXECUTABLE} ${EXECUTABLE}.1 audioDB.o
	-sh -c "cd tests && sh ./clean.sh"

test: ${EXECUTABLE}
	-sh -c "cd tests && sh ./run-tests.sh"
