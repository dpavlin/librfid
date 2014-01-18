dnl Process this file with autoconf to create configure.

AC_INIT([librfid], [0.2.0])
AC_CANONICAL_SYSTEM
AM_INIT_AUTOMAKE([subdir-objects])

AC_PROG_CC
AM_PROG_CC_C_O
AC_C_CONST
AC_EXEEXT
AC_LIBTOOL_WIN32_DLL
AM_PROG_LIBTOOL
AC_SUBST(LIBTOOL_DEPS)

AC_MSG_CHECKING([for build environment])
case "$target_alias" in
*-mingw*)	BUILDENV=WIN32;;
*-cygwin*)	BUILDENV=WIN32;;
*)		BUILDENV=UNIX;;
esac
AC_MSG_RESULT($BUILDENV)
AC_SUBST(BUILDENV)
AM_CONDITIONAL(ENABLE_WIN32, test "$BUILDENV" == "WIN32")
AM_CONDITIONAL(DISABLE_WIN32, test "$BUILDENV" != "WIN32")

AC_C_BIGENDIAN(AC_DEFINE(RFID_BIG_ENDIAN_BITFIELD, 1, [Big-endian bitfield]),,)

AC_ARG_ENABLE(ccid,
	[  --enable-ccid        Include internal CCID driver for CM5121],
	[MY_CCID=1], [MY_CCID=0])
# AC_SUBST(MY_CCID)
AM_CONDITIONAL(ENABLE_CCID, test "$MY_CCID" == "1")

dnl Enable/disable openct
AC_ARG_ENABLE(openct,
        AC_HELP_STRING([--enable-openct], [Use (patched) OpenCT for CM5121 CCID access. [[default=no]]]),
        [ENABLE_OPENCT="${enableval}"],
        [ENABLE_OPENCT="no"])

AC_ARG_ENABLE(spidev,
	[  --enable-spidev      Enable spidev reader],
	[ENABLE_SPIDEV=1], [ENABLE_SPIDEV=0])
AM_CONDITIONAL(ENABLE_SPIDEV, test "$ENABLE_SPIDEV" == "1")

OPENCT_MSG=no
AC_PROVIDE_IFELSE([PKG_CHECK_MODULES)], [
	if test "x${ENABLE_OPENCT}" = "xyes"; then
		PKG_CHECK_MODULES(OPENCT, [libopenct], [
			OPENCT_MSG=yes
			AC_DEFINE(HAVE_OPENCT, 1, [Have OpenCT libraries and header files])
		], [
			OPENCT_MSG=no
		])
		AC_SUBST(OPENCT_CFLAGS)
		AC_SUBST(OPENCT_LIBS)
	fi ], [
		AC_SUBST(OPENCT_CFLAGS)
		AC_SUBST(OPENCT_LIBS)
	])
AM_CONDITIONAL(HAVE_OPENCT, test "x$OPENCT_MSG" = "xyes")

AC_ARG_WITH(firmware,
	[  --with-firmware=PATH	Compile for running inside firmware],
	[FIRMWARE_PATH="$withval"], [FIRMWARE_PATH=""])
AC_SUBST(FIRMWARE_PATH)
AM_CONDITIONAL(ENABLE_FIRMWARE, test "$FIRMWARE_PATH" != "")

AC_ARG_ENABLE(static,
	[  --enable-static	Don't use dynamic allocations at all],
	[ENABLE_STATIC=1], [ENABLE_STATIC=0]) 
AM_CONDITIONAL(ENABLE_STATIC, test "$ENABLE_STATIC" == "1")

AC_ARG_WITH()

AC_CHECK_LIB(usb, usb_close, [HAVE_LIBUSB=1], [HAVE_LIBUSB=0])
AM_CONDITIONAL(HAVE_LIBUSB, test "$HAVE_LIBUSB" == "1")

dnl Output the makefile
AC_OUTPUT(Makefile etc/Makefile etc/udev/Makefile src/Makefile include/Makefile include/librfid/Makefile utils/Makefile src/librfid.pc win32/Makefile)
