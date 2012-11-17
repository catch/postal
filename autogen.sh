#!/bin/sh

(libtool --version) < /dev/null > /dev/null 2>&1 || {
	# GNU libtool wasn't found as "libtool",
	# so we check if it's known as "glibtool"
	(glibtool --version) < /dev/null > /dev/null 2>&1 || {
		echo
		echo "You must have libtool installed to compile libvirt."
		echo "Download the appropriate package for your distribution,"
		echo "or see http://www.gnu.org/software/libtool";
		DIE=1
	}
	# These are only used if glibtool is what we want
	export LIBTOOL=glibtool
	export LIBTOOLIZE=glibtoolize
}

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
	echo "Now type 'make' to compile Postal"
)
