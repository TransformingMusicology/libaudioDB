INCLUDE=include
BUILD_DIR=build
LIBNAME=audioDB
SOVERSION=0
MINORVERSION=0
LIBRARY_FULL=lib$(LIBNAME).so.$(SOVERSION).$(MINORVERSION)
LIBRARY_VERS=lib$(LIBNAME).so.$(SOVERSION)
LIBRARY=lib$(LIBNAME).so
ARCH_FLAGS=


ifeq ($(shell uname),Darwin)
override LIBRARY_FULL=lib$(LIBNAME).$(SOVERSION).$(MINORVERSION).dylib
override LIBRARY_VERS=lib$(LIBNAME).$(SOVERSION).dylib
override LIBRARY=lib$(LIBNAME).dylib
ifeq ($(shell sysctl -n hw.optional.x86_64),1)
override ARCH_FLAGS+=-arch x86_64
endif
endif

all: $(LIBRARY_FULL) $(LIBRARY_VERS) $(LIBRARY) test1

$(LIBRARY_FULL):
	-ln -s ../../$(BUILD_DIR)/$(LIBRARY_FULL) $@

$(LIBRARY_VERS):
	-ln -s ../../$(BUILD_DIR)/$(LIBRARY_FULL) $@

$(LIBRARY): $(LIBRARY_VERS)
	-ln -s $< $@

test1: prog1.c ../test_utils_lib.h ../../$(INCLUDE)/audioDB/audioDB_API.h
	gcc -g -std=c99 -Wall -Werror $(ARCH_FLAGS) -I.. -I../../$(INCLUDE) -laudioDB -L$(BUILD_DIR) -Wl,-rpath,. -o $@ $<

clean:
	-rm $(LIBRARY_FULL) $(LIBRARY_VERS) $(LIBRARY)
