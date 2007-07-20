
CFLAGS=-ggdb
LIBDIR=-L/usr/include 
LIBS=-lgsoap++

all: audioDB.h audioDB.cpp soapServer.cpp soapClient.cpp soapC.cpp Makefile
	g++ -o audioDB ${CFLAGS} audioDB.cpp soapServer.cpp soapClient.cpp soapC.cpp ${CFLAGS} ${LIBDIR} ${LIBS}

