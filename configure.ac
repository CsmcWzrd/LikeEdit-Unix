AC_PREREQ([2.69])
AC_INIT([LikeEdit],[1.1.0],[https://github.com/CsmcWzrd/LikeConio/issues])
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([foreign subdir-objects])
AC_PROG_CC
AC_PROG_INSTALL
AC_USE_SYSTEM_EXTENSIONS
AC_ARG_WITH([system-conio],
  [AS_HELP_STRING([--with-system-conio],[link against installed LikeConio 1.0])],
  [],[with_system_conio=no])
AS_IF([test "x$with_system_conio" = "xyes"], [
  AC_CHECK_HEADER([conio.h],[],[AC_MSG_ERROR([conio.h not found; install LikeConio])])
  AC_CHECK_LIB([conio],[conio_init],[CONIO_LIBS="-lconio"],
    [AC_MSG_ERROR([LikeConio library not found; set LDFLAGS or use fallback])])
], [
  AC_CHECK_HEADER([ncurses.h],[],[AC_MSG_ERROR([ncurses.h is required])])
  AC_SEARCH_LIBS([wget_wch],[ncursesw ncurses curses],
    [AS_IF([test "x$ac_cv_search_get_wch" = "xnone required"],
      [CONIO_LIBS=""],[CONIO_LIBS="$ac_cv_search_get_wch"])],
    [AC_MSG_ERROR([wide-character ncurses is required for UTF-8 input])])
])
AC_SUBST([CONIO_LIBS])
AM_CONDITIONAL([SYSTEM_CONIO],[test "x$with_system_conio" = "xyes"])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
