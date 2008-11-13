all: libaudioDB.so.0 libaudioDB.so test1

libaudioDB.so.0:
	-ln -s ../../libaudioDB.so.0.0 $@

libaudioDB.so: libaudioDB.so.0
	-ln -s $< $@

test1: prog1.c
	gcc -Wall -laudioDB -L. -Wl,-rpath,. -o $@ $<

clean:
	-rm libaudioDB.so.0 libaudioDB.so