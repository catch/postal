#!/bin/sh

package="Postal"

if [ "`uname -s`" = "Darwin" ]; then
	export LIBTOOL=glibtool
	export LIBTOOLIZE=glibtoolize
else
	export LIBTOOL=libtool
	export LIBTOOLIZE=libtoolize
fi

DIE=0

($LIBTOOL --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have libtool installed to compile $package."
	echo "Download the appropriate package for your system,"
	echo "or get the source from one of the GNU ftp sites"
	echo "listed in http://www.gnu.org/order/ftp.html"
	DIE=1
}


if test "$DIE" -eq 1; then
	exit 1
fi

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

test -f "configure.ac" || {
	echo "You must run this script in the top-level postal directory"
	exit 1
}

autoreconf -v --install || exit $?

test -n "$NOCONFIGURE" ||
(
	"$srcdir/configure" "$@" &&
	echo "Now type 'make' to compile ${package}"
)
