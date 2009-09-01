# -*- Mode: Makefile -*-
#
# Makefile.am - top level automake file for Redland
#
# Copyright (C) 2000-2009, David Beckett http://www.dajobe.org/
# Copyright (C) 2000-2004, Copyright (C) 2000-2006, University of Bristol, UK http://www.bristol.ac.uk/
# 
# This package is Free Software and part of Redland http://librdf.org/
# 
# It is licensed under the following three licenses as alternatives:
#   1. GNU Lesser General Public License (LGPL) V2.1 or any newer version
#   2. GNU General Public License (GPL) V2 or any newer version
#   3. Apache License, V2.0 or any newer version
# 
# You may not use this file except in compliance with at least one of
# the above three licenses.
# 
# See LICENSE.html or LICENSE.txt at the top of this package for the
# complete terms and further detail along with the license texts for
# the licenses in COPYING.LIB, COPYING and LICENSE-2.0.txt respectively.
# 
# 

plugindir = $(libdir)/redland

plugin_LTLIBRARIES = librdf_storage_audiodb.la
librdf_storage_audiodb_la_SOURCES = rdf_storage_audiodb.c
librdf_storage_audiodb_la_CPPFLAGS = 
librdf_storage_audiodb_la_LIBADD = -laudioDB
librdf_storage_audiodb_la_LDFLAGS = -module -avoid-version

# Use tar, whatever it is called (better be GNU tar though)
TAR=@TAR@

# Why is this not in the default makefile?
CC=@CC@

AM_CPPFLAGS=@LIBRDF_INTERNAL_CPPFLAGS@ @LIBRDF_CPPFLAGS@ @DEPS_CFLAGS@
AM_CFLAGS=@LIBRDF_INTERNAL_CPPFLAGS@ @LIBRDF_CPPFLAGS@ @DEPS_CFLAGS@ $(MEM)


AM_CPPFLAGS += -DPKGDATADIR="\"$(pkgdatadir)\""
AM_CFLAGS += -DLIBRDF_MODULE_DIR="\"$(libdir)/redland\""
AM_LDFLAGS = @LIBRDF_INTERNAL_LIBS@ @DEPS_LIBS@

LOCAL_LIB_DIR=lib

# rule for building tests in one step
COMPILE_LINK = $(LIBTOOL) --tag=CC --mode=link $(CCLD) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) $(AM_LDFLAGS) $(LDFLAGS) -o $@

@SET_MAKE@

${top_build_prefix}libltdl/libltdlc.la:
	cd  ${top_build_prefix}libltdl && R(MAKE) libltdlc.la

