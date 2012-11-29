dnl **************************************************************************
dnl Initialize Autoconf and Automake
dnl **************************************************************************
AC_PREREQ([2.61])
AC_INIT([postal], [0.1.0])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([-Wno-portability])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES],
                            [AC_SUBST([AM_DEFAULT_VERBOSITY], [1])])

AS_AM_REALLY_SILENT

# Check for programs
AC_PROG_CC

# Initialize libtool
LT_PREREQ([2.2])
LT_INIT


dnl **************************************************************************
dnl Check for Required Modules
dnl **************************************************************************
PKG_CHECK_MODULES(GIO,   [gio-2.0 >= 2.32])
PKG_CHECK_MODULES(SOUP,  [libsoup-2.4 >= 2.26.1])
PKG_CHECK_MODULES(JSON,  [json-glib-1.0 >= 0.14.0])


dnl **************************************************************************
dnl Check for Redis Support
dnl **************************************************************************
AC_ARG_ENABLE([redis],
	      [AS_HELP_STRING([--enable-redis=@<:@no/yes@:>@],
			     [Add support for delivering metrics to redis.])],
	      [],
	      [enable_redis=yes])
AS_CASE([$enable_redis],
	[yes], [
		AC_CHECK_HEADERS([hiredis/hiredis.h hiredis/async.h],
		                 [AC_DEFINE([ENABLE_REDIS], [], [Support for Redis.])],
		                 [AC_MSG_ERROR([Please install hiredis devel packages])])
	],
	[no], [
	],
	[AC_MSG_ERROR([Unknown argument to --enable-redis])]
)
AM_CONDITIONAL([ENABLE_REDIS], [test "x$enable_redis" = xyes])


AC_ARG_ENABLE([unistring],
	      [AS_HELP_STRING([--enable-unistring=@<:@no/yes@:>@],
			     [Add support for full UTF-8.])],
	      [],
	      [enable_unistring=no])
AS_CASE([$enable_unistring],
	[yes], [
		AC_CHECK_HEADERS([unistr.h],
		                 [AC_DEFINE([ENABLE_UNISTRING], [], [Support for libunistring.])],
		                 [AC_MSG_ERROR([Please install libunistring devel package.])])
		utf8_support="libunistring; full UTF-8 support."
	],
	[no], [
		utf8_support="glib; only modified UTF-8."
	],
	[AC_MSG_ERROR([Unknown argument to --enable-unistring])]
)
AM_CONDITIONAL([ENABLE_UNISTRING], [test "x$enable_unistring" = xyes])


dnl **************************************************************************
dnl Enable extra debugging options
dnl **************************************************************************
AC_ARG_ENABLE([debug],
	      [AS_HELP_STRING([--enable-debug=@<:@no/minimum/yes@:>@],
	      		      [turn on debugging @<:@default=yes@:>@])],
	      		      [],
	      		      [enable_debug=yes])
AS_CASE([$enable_debug],
	[yes], [
		CFLAGS="$CFLAGS -g"
		CFLAGS="$CFLAGS -Wall"
		CFLAGS="$CFLAGS -Werror"
		CFLAGS="$CFLAGS -Wold-style-definition"
		CFLAGS="$CFLAGS -Wdeclaration-after-statement"
		CFLAGS="$CFLAGS -Wredundant-decls"
		CFLAGS="$CFLAGS -Wmissing-noreturn"
		CFLAGS="$CFLAGS -Wcast-align"
		CFLAGS="$CFLAGS -Wwrite-strings"
		CFLAGS="$CFLAGS -Winline"
		CFLAGS="$CFLAGS -Wformat-nonliteral"
		CFLAGS="$CFLAGS -Wformat-security"
		CFLAGS="$CFLAGS -Wswitch-enum"
		CFLAGS="$CFLAGS -Wswitch-default"
		CFLAGS="$CFLAGS -Winit-self"
		CFLAGS="$CFLAGS -Wmissing-include-dirs"
		CFLAGS="$CFLAGS -Wundef"
		CFLAGS="$CFLAGS -Waggregate-return"
		CFLAGS="$CFLAGS -Wmissing-format-attribute"
		CFLAGS="$CFLAGS -Wnested-externs"
		CFLAGS="$CFLAGS -Wshadow"
	],
	[minimum], [
		CFLAGS="$CFLAGS -DG_DISABLE_CAST_CHECKS"
	],
	[no], [
		CFLAGS="$CFLAGS -DG_DISABLE_ASSERT"
		CFLAGS="$CFLAGS -DG_DISABLE_CHECKS"
		CFLAGS="$CFLAGS -DG_DISABLE_CAST_CHECKS"
	],
	[AC_MSG_ERROR([Unknown argument to --enable-debug])]
)
AC_ARG_ENABLE([trace],
	      [AS_HELP_STRING([--enable-trace=@<:@no/minimum/yes@:>@],
	      		      [turn on tracing @<:@default=yes@:>@])],
	      		      [],
	      		      [enable_trace=yes])
AS_CASE([$enable_trace],
	[yes], [
		CFLAGS="$CFLAGS -DMONGO_TRACE"
		CFLAGS="$CFLAGS -DPOSTAL_TRACE"
		CFLAGS="$CFLAGS -DPUSH_TRACE"
		CFLAGS="$CFLAGS -DREDIS_TRACE"
	],
	[no], [],
	[AC_MSG_ERROR([Unknown argument to --enable-trace])]
)


dnl **************************************************************************
dnl Unit Tests
dnl **************************************************************************
AC_PATH_PROG([GTESTER], [gtester])
AC_PATH_PROG([GTESTER_REPORT], [gtester-report])
AM_CONDITIONAL(ENABLE_GLIB_TEST, test "x$enable_glibtest" = "xyes")


dnl **************************************************************************
dnl Output
dnl **************************************************************************
AC_CONFIG_FILES([
	Makefile
])
AC_OUTPUT


echo ""
echo "Postal $VERSION"
echo ""
echo "  Prefix .................: ${prefix}"
echo "  Debug ..................: ${enable_debug}"
echo "  Tracing ................: ${enable_trace}"
echo "  Redis ..................: ${enable_redis}"
echo "  UTF-8 ..................: ${utf8_support}"
echo ""
echo "Dependencies"
echo ""
echo "  GIO ....................: $(pkg-config --modversion gio-2.0)"
echo "  JSON ...................: $(pkg-config --modversion json-glib-1.0)"
echo "  SOUP ...................: $(pkg-config --modversion libsoup-2.4)"
echo ""
