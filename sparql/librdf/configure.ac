dnl -*- Mode: autoconf -*-
dnl
dnl configure.ac - top level autoconf file for Redland
dnl (Process this file with autoconf to produce a configure script.)
dnl
dnl Copyright (C) 2000-2009 David Beckett http://www.dajobe.org/
dnl Copyright (C) 2000-2005 University of Bristol, UK http://www.bristol.ac.uk/
dnl 
dnl This package is Free Software and part of Redland http://librdf.org/
dnl 
dnl It is licensed under the following three licenses as alternatives:
dnl   1. GNU Lesser General Public License (LGPL) V2.1 or any newer version
dnl   2. GNU General Public License (GPL) V2 or any newer version
dnl   3. Apache License, V2.0 or any newer version
dnl 
dnl You may not use this file except in compliance with at least one of
dnl the above three licenses.
dnl 
dnl See LICENSE.html or LICENSE.txt at the top of this package for the
dnl complete terms and further detail along with the license texts for
dnl the licenses in COPYING.LIB, COPYING and LICENSE-2.0.txt respectively.
dnl 
dnl 


AC_INIT([Redland AudioDB Storage Module], 0.0.1, http://omras2.doc.gold.ac.uk/software/audiodb/, redland_audioDB)
AC_PREREQ(2.53)
AC_REVISION($Revision$)dnl

AM_INIT_AUTOMAKE([1.7])
AM_MAINTAINER_MODE

AC_CANONICAL_HOST


changequote(<<, >>)dnl
version_major=`echo $VERSION | sed -e 's/^\([^\.]*\)\.\([^\.]*\)\.\(.*\)$/\1/'`
version_minor=`echo $VERSION | sed -e 's/^\([^\.]*\)\.\([^\.]*\)\.\(.*\)$/\2/'`
version_release=`echo $VERSION | sed -e 's/^\([^\.]*\)\.\([^\.]*\)\.\(.*\)$/\3/'`
changequote([, ])dnl

version_decimal=`expr $version_major \* 10000 + $version_minor \* 100 + $version_release`

AC_DEFINE_UNQUOTED(LIBRDF_VERSION_MAJOR, $version_major, [Major version number])
AC_DEFINE_UNQUOTED(LIBRDF_VERSION_MINOR, $version_minor, [Minor version number])
AC_DEFINE_UNQUOTED(LIBRDF_VERSION_RELEASE, $version_release, [Release version number])
AC_DEFINE_UNQUOTED(LIBRDF_VERSION_DECIMAL, $version_decimal, [Release version as a decimal])

LIBRDF_LIBTOOL_VERSION=0:0:0
AC_SUBST(LIBRDF_LIBTOOL_VERSION)

dnl Checks for programs.
AM_SANITY_CHECK
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

m4_undefine([AC_PROG_CXX])
m4_defun([AC_PROG_CXX],[])
m4_undefine([AC_PROG_F77])
m4_defun([AC_PROG_F77],[])

# Libtool and LTDL initialising
LT_CONFIG_LTDL_DIR([libltdl])
LT_INIT([dlopen])
LTDL_INIT([convenience])

# Find a tar command for 'make dist'
AC_CHECK_PROGS(TAR, gnutar gtar tar)

AC_CHECK_PROGS(PERL, perl)
# Gnome
AC_CHECK_PROGS(PKG_CONFIG, pkg-config)


AM_MISSING_PROG(ACLOCAL, aclocal, $missing_dir)
AM_MISSING_PROG(AUTOCONF, autoconf, $missing_dir)
AM_MISSING_PROG(AUTOMAKE, automake, $missing_dir)
AM_MISSING_PROG(AUTOHEADER, autoheader, $missing_dir)


# Externally linked libraries - appear in redland-config
# -Dfoo -Idir
LIBRDF_CPPFLAGS=$CPPFLAGS
# -Ldir
LIBRDF_LDFLAGS=$LDFLAGS
# -llib
LIBRDF_LIBS=$LIBS

# Internally linked libraries - never given to users
# -Dfoo -Idir
LIBRDF_INTERNAL_CPPFLAGS=
# -Ldir -llib
LIBRDF_INTERNAL_LIBS=

# Libraries linked externally also may be internal - presently just
# raptor and rasqal but in future may include others
# Currently only substituted into redland-src-config.
# -Dfoo -Idir
LIBRDF_EXTERNAL_CPPFLAGS=
# -Ldir -llib
LIBRDF_EXTERNAL_LIBS=

# do these first because somehow they get messed up by bdb checks
AC_CHECK_TYPES([byte])
AC_CHECK_TYPES([u32])
AC_CHECK_TYPES([u64])

AC_CHECK_SIZEOF(unsigned char, 1)
AC_CHECK_SIZEOF(unsigned int, 4)
AC_CHECK_SIZEOF(unsigned long, 4)
AC_CHECK_SIZEOF(unsigned long long, 8)

CPPFLAGS="$LIBRDF_CPPFLAGS"
LDFLAGS="$LIBRDF_LDFLAGS"
LIBS="$LIBRDF_LIBS"


dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_BIGENDIAN

dnl Checks for URI resolvers

CPPFLAGS="$LIBRDF_CPPFLAGS"
LDFLAGS="$LIBRDF_LDFLAGS"
LIBS="$LIBRDF_LIBS"


dnl Checks for RDF parsers
parser_modules=raptor
rdf_parsers_available=

# Check for raptor
AC_MSG_CHECKING(for raptor)

RAPTOR_MIN_VERSION=1.4.17
AC_SUBST(RAPTOR_MIN_VERSION)
if $PKG_CONFIG raptor --exists; then
  :
else
  AC_MSG_ERROR(Raptor is not installed - see http://librdf.org/raptor/ to get a version newer than $RAPTOR_MIN_VERSION)
fi

RAPTOR_VERSION=`$PKG_CONFIG raptor --modversion 2>/dev/null`

if $PKG_CONFIG raptor --atleast-version=$RAPTOR_MIN_VERSION; then
  :
else
  AC_MSG_ERROR(Installed raptor $RAPTOR_VERSION is too old - need $RAPTOR_MIN_VERSION)
fi

LIBRDF_EXTERNAL_CPPFLAGS="`$PKG_CONFIG raptor --cflags` $LIBRDF_EXTERNAL_CPPFLAGS"
LIBRDF_EXTERNAL_LIBS="`$PKG_CONFIG raptor --libs` $LIBRDF_EXTERNAL_LIBS"

rdf_parsers_available="$parser_available raptor(system $RAPTOR_VERSION)"
AC_MSG_RESULT(system $RAPTOR_VERSION)

# Check for rasqal
AC_MSG_CHECKING(for rasqal)

RASQAL_MIN_VERSION=0.9.16
RASQAL_MAX_VERSION=0.9.99
AC_SUBST(RASQAL_MIN_VERSION)
AC_SUBST(RASQAL_MAX_VERSION)
if $PKG_CONFIG rasqal --exists; then
  :
else
  AC_MSG_ERROR(Rasqal is not installed - see http://librdf.org/rasqal/ to get a version in range $RASQAL_MIN_VERSION to $RASQAL_MAX_VERSION)
fi

RASQAL_VERSION=`$PKG_CONFIG rasqal --modversion 2>/dev/null`

if $PKG_CONFIG rasqal --atleast-version=$RASQAL_MIN_VERSION --max-version=$RASQAL_MAX_VERSION; then
  :
else
  AC_MSG_ERROR(Installed rasqal $RASQAL_VERSION is not in range $RASQAL_MIN_VERSION to $RASQAL_MAX_VERSION)
fi

LIBRDF_EXTERNAL_CPPFLAGS="`$PKG_CONFIG rasqal --cflags` $LIBRDF_EXTERNAL_CPPFLAGS"
LIBRDF_EXTERNAL_LIBS="`$PKG_CONFIG rasqal --libs` $LIBRDF_EXTERNAL_LIBS"

rdf_query="rasqal(system $RASQAL_VERSION)"
AC_MSG_RESULT(system $RASQAL_VERSION)

LIBS="$LIBS"
CPPFLAGS="$CPPFLAGS"
LDFLAGS="$LDFLAGS"

LIBRDF_LIBTOOLLIBS=librdf.la

AC_SUBST(LIBRDF_EXTERNAL_CPPFLAGS)
AC_SUBST(LIBRDF_EXTERNAL_LIBS)

AC_SUBST(LIBRDF_INTERNAL_CPPFLAGS)
AC_SUBST(LIBRDF_INTERNAL_LIBS)

AC_SUBST(LIBRDF_CPPFLAGS)
AC_SUBST(LIBRDF_LDFLAGS)
AC_SUBST(LIBRDF_LIBTOOLLIBS)
AC_SUBST(LIBRDF_PKGCONFIG_PRIVATE_LIBS)

AC_SUBST(MEM)
AC_SUBST(MEM_LIBS)

abs_top_srcdir=`cd $srcdir; pwd`
AC_SUBST(abs_top_srcdir)
abs_top_builddir=`pwd`
AC_SUBST(abs_top_builddir)

AC_CONFIG_FILES([Makefile src/Makefile])

AC_OUTPUT
