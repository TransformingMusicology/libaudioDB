PKGCONFIG=pkg-config
INCLUDE=include
SRC=src
BUILD_DIR=build
INC_PREFIX=audioDB
ADB_INCLUDE=-I$(INCLUDE)
GSL_INCLUDE=$(shell $(PKGCONFIG) --cflags gsl)
LIBGSL=$(shell $(PKGCONFIG) --libs gsl)

TESTDIRS=libtests

PREFIX=/usr/local
LIBDIR=$(PREFIX)/lib
INCLUDEDIR=$(PREFIX)/include

_LIBOBJS=lock.o pointpair.o create.o open.o power.o l2norm.o insert.o status.o query.o dump.o close.o index-utils.o query-indexed.o liszt.o retrieve.o lshlib.o multiprobe.o sample.o
LIBOBJS=$(patsubst %,$(BUILD_DIR)/%,$(_LIBOBJS))
_INCLUDES=accumulator.h audioDB_API.h dbaccumulator.h multiprobe.h pertrackaccumulator.h accumulators.h audioDB-internals.h lshlib.h nearestaccumulator.h pointpair.h
INCLUDES=$(patsubst %,$(INCLUDE)/$(INC_PREFIX)/%,$(_INCLUDES))

LIBNAME=audioDB

SOVERSION=0
MINORVERSION=0
LIB_BUILD_MODIFIED=$(shell git diff HEAD --no-ext-diff --quiet --exit-code ; if [ $$? -eq "1" ]; then echo "-modified"; fi)
LIB_BUILD_ID=$(shell git rev-parse HEAD)$(LIB_BUILD_MODIFIED)
LIB_BUILD_DATE=$(shell date +'%Y-%m-%dT%H:%M:%S')
LIB_BUILD_INFO_CFLAGS=-DLIB_BUILD_DATE="\"$(LIB_BUILD_DATE)\"" -DLIB_BUILD_ID="\"$(LIB_BUILD_ID)\""

LIBRARY=lib$(LIBNAME).so.$(SOVERSION).$(MINORVERSION)
SHARED_LIB_FLAGS=-shared -Wl,-soname,lib$(LIBNAME).so.$(SOVERSION)

override CFLAGS+=-g -O3 -fPIC 

# set to generate profile (gprof) and coverage (gcov) info
#override CFLAGS+=-fprofile-arcs -ftest-coverage -pg

# set to DUMP hashtables on QUERY load
#override CFLAGS+=-DLSH_DUMP_CORE_TABLES

ifeq ($(shell uname),Linux)
override CFLAGS+=-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
endif

ifeq ($(shell uname),Darwin)
ifeq ($(shell sysctl -n hw.optional.x86_64),1)
override CFLAGS+=-arch x86_64
endif
override LIBRARY=lib$(LIBNAME).$(SOVERSION).$(MINORVERSION).dylib
override SHARED_LIB_FLAGS=-dynamiclib -current_version $(SOVERSION) -install_name $(LIBRARY)
endif

.PHONY: all clean test install 

all: $(BUILD_DIR) build_info $(LIBRARY)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

build_info:
	$(CXX) -o $(BUILD_DIR)/build_info.o -c $(CFLAGS) $(LIB_BUILD_INFO_CFLAGS) $(GSL_INCLUDE) $(ADB_INCLUDE) -Wall $(SRC)/build_info.cpp

$(LIBOBJS): $(BUILD_DIR)/%.o: $(SRC)/%.cpp $(INCLUDE)/$(INC_PREFIX)/audioDB_API.h $(INCLUDE)/$(INC_PREFIX)/audioDB-internals.h $(INCLUDE)/$(INC_PREFIX)/accumulator.h $(INCLUDE)/$(INC_PREFIX)/accumulators.h
	$(CXX) -o $@ -c $(CFLAGS) $(GSL_INCLUDE) $(ADB_INCLUDE) -Wall $<

$(BUILD_DIR)/%.o: $(SRC)/%.cpp audioDB.h audioDB_API.h adb.nsmap lshlib.h
	$(CXX) -o $@ -c $(CFLAGS) $(GSL_INCLUDE) $(ADB_INCLUDE) -Wall  $<

$(LIBRARY): $(LIBOBJS)
	$(CXX) $(SHARED_LIB_FLAGS) -o $(BUILD_DIR)/$(LIBRARY) $(CFLAGS) $^ $(LIBGSL)

tags:
	ctags $(SRC)/*.cpp $(INCLUDE)/$(INC_PREFIX)/*.h

testclean: 
	-for d in $(TESTDIRS); do (cd $$d && sh ./clean.sh); done

clean: testclean
	-rm adb.*
	-rm $(LIBOBJS)
	-rm $(BUILD_DIR)/build_info.o
	-rm $(BUILD_DIR)/xthresh
	-rm $(BUILD_DIR)/$(LIBRARY)
	-rm tags
	-rm $(BUILD_DIR)/$(LIBNAME).pc
	-rmdir $(BUILD_DIR)

distclean: clean
	-rm $(BUILD_DIR)/*.o
	-rm -rf audioDB.dump

test: $(LIBRARY)
	for d in $(TESTDIRS); do (cd $$d && sh ./run-tests.sh); done

xthresh: $(SRC)/xthresh.c
	$(CC) -o $(BUILD_DIR)/$@ $(CFLAGS) $(GSL_INCLUDE) $(ADB_INCLUDE) $< $(LIBGSL)

$(LIBNAME).pc:
	./make-pc.sh "$(LIBNAME)" "$(LIBDIR)" "$(INCLUDEDIR)" $(SOVERSION) $(MINORVERSION) > $(BUILD_DIR)/$(LIBNAME).pc

install: $(LIBNAME).pc
	mkdir -m755 -p $(LIBDIR)/pkgconfig $(INCLUDEDIR)/$(INC_PREFIX)
	install -m644 $(BUILD_DIR)/$(LIBRARY) $(LIBDIR)
	install -m644 $(INCLUDES) $(INCLUDEDIR)/$(INC_PREFIX)
	install $(BUILD_DIR)/$(LIBNAME).pc $(LIBDIR)/pkgconfig
	ln -sf $(LIBRARY) $(LIBDIR)/lib$(LIBNAME).so
ifneq ($(shell uname),Darwin)
	ldconfig -n $(LIBDIR)
endif
