CFLAGS=-O3
LIBS=-lgsoap++

EXECUTABLE=audioDB

all: ${EXECUTABLE}

${EXECUTABLE}.1: ${EXECUTABLE}
	help2man ./${EXECUTABLE} > ${EXECUTABLE}.1

README.txt: ${EXECUTABLE}
	./${EXECUTABLE} --help > README.txt

cmdline.c cmdline.h: gengetopt.in
	gengetopt <gengetopt.in

soapServer.cpp soapClient.cpp soapC.cpp: audioDBws.h
	soapcpp2 audioDBws.h

audioDB.o: audioDB.cpp audioDB.h soapServer.cpp soapClient.cpp soapC.cpp cmdline.c cmdline.h
	g++ -c ${CFLAGS} -Wall -Werror audioDB.cpp

${EXECUTABLE}: audioDB.o soapServer.cpp soapClient.cpp soapC.cpp cmdline.c cmdline.h
	g++ -o ${EXECUTABLE} ${CFLAGS} audioDB.o soapServer.cpp soapClient.cpp soapC.cpp cmdline.c ${LIBS}

clean:
	-rm cmdline.c cmdline.h
	-rm soapServer.cpp soapClient.cpp soapC.cpp soapObject.h soapStub.h soapProxy.h soapH.h soapServerLib.cpp soapClientLib.cpp
	-rm adb.nsmap adb.xsd adb.wsdl adb.query.req.xml adb.query.res.xml adb.status.req.xml adb.status.res.xml
	-rm README.txt
	-rm ${EXECUTABLE} ${EXECUTABLE}.1 audioDB.o
	-sh -c "cd tests && sh ./clean.sh"
