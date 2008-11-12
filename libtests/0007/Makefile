CC=gcc
CFLAGS+=-O3 -g
RM=rm -rf


all:	test1


test1:	prog1.c 
	gcc -Wall -o test1 prog1.c -laudioDB_API -L../..

clean:
	$(RM) test1
