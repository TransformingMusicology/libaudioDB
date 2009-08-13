all: mod_audiodb.so

SOURCE = mod_audiodb.c

HEAD = mod_audiodb.h 
ACLOCAL_AMFLAGS = -I m4
EXTRA_DIST = $(SOURCE) $(HEAD)

mod_audiodb.so: $(SOURCE) $(HEAD)
		@APXS@ -Wc,-g\ -Wall `@APREQ2@ --ldflags --libs --includes` `pkg-config rasqal --cflags --libs` -lrdf -laudioDB -I/usr/include/ $(GLIB_CFLAGS) -c $(SOURCE)

install: mod_audiodb.so
		@APXS@ -i -a `test -f mod_audiodb.so && echo "mod_audiodb.so" || echo "mod_audiodb.la"`

