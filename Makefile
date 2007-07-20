CFLAGS=-ggdb
LIBS=-lgsoap++

all: audioDB

cmdline.c cmdline.h: gengetopt.in
	gengetopt <gengetopt.in

soapServer.cpp soapClient.cpp soapC.cpp: audioDBws.h
	soapcpp2 audioDBws.h

audioDB: audioDB.h audioDB.cpp soapServer.cpp soapClient.cpp soapC.cpp cmdline.c cmdline.h
	g++ -o audioDB ${CFLAGS} audioDB.cpp soapServer.cpp soapClient.cpp soapC.cpp cmdline.c ${LIBS}
