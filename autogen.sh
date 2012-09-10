#!/bin/sh

[ -f configure.ac ] || {
  echo "autogen.sh: run this command only at the top of a recpt1 source tree."
  exit 1
}

DIE=0

(autoconf --version) < /dev/null > /dev/null 2>&1 || {
  echo
  echo "You must have autoconf installed to compile recpt1."
  echo "Get ftp://ftp.gnu.org/pub/gnu/autoconf/autoconf-2.62.tar.gz"
  echo "(or a newer version if it is available)"
  DIE=1
  NO_AUTOCONF=yes
}

(automake --version) < /dev/null > /dev/null 2>&1 || {
  echo
  echo "You must have automake installed to compile recpt1."
  echo "Get ftp://ftp.gnu.org/pub/gnu/automake/automake-1.10.1.tar.gz"
  echo "(or a newer version if it is available)"
  DIE=1
  NO_AUTOMAKE=yes
}

# if no automake, don't bother testing for aclocal
test -n "$NO_AUTOMAKE" || (aclocal --version) < /dev/null > /dev/null 2>&1 || {
  echo
  echo "**Error**: Missing \`aclocal'.  The version of \`automake'"
  echo "installed doesn't appear recent enough."
  echo "Get ftp://ftp.gnu.org/pub/gnu/automake/automake-1.10.1.tar.gz"
  echo "(or a newer version if it is available)"
  DIE=1
}

# if no autoconf, don't bother testing for autoheader
test -n "$NO_AUTOCONF" || (autoheader --version) < /dev/null > /dev/null 2>&1 || {
  echo
  echo "**Error**: Missing \`autoheader'.  The version of \`autoheader'"
  echo "installed doesn't appear recent enough."
  echo "Get ftp://ftp.gnu.org/pub/gnu/autoconf/autoconf-2.62.tar.gz"
  echo "(or a newer version if it is available)"
  DIE=1
}

if test "$DIE" -eq 1; then
        exit 1
fi

echo "Generating configure script and Makefiles for recpt1."

echo "Running aclocal ..."
aclocal -I .
echo "Running autoheader ..."
autoheader
echo "Running autoconf ..."
autoconf
