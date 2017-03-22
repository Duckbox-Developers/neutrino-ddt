AC_DEFUN([TUXBOX_APPS],[
AM_CONFIG_HEADER(config.h)
AM_MAINTAINER_MODE

AC_GNU_SOURCE

AC_ARG_WITH(target,
	[  --with-target=TARGET    target for compilation [[native,cdk]]],
	[TARGET="$withval"],[TARGET="native"])

AC_ARG_WITH(targetprefix,
	[  --with-targetprefix=PATH  prefix relative to target root (only applicable in cdk mode)],
	[TARGET_PREFIX="$withval"],[TARGET_PREFIX=""])

AC_ARG_WITH(debug,
	[  --without-debug         disable debugging code],
	[DEBUG="$withval"],[DEBUG="yes"])

if test "$DEBUG" = "yes"; then
	DEBUG_CFLAGS="-g3 -ggdb"
	AC_DEFINE(DEBUG,1,[Enable debug messages])
fi

AC_MSG_CHECKING(target)

if test "$TARGET" = "native"; then
	AC_MSG_RESULT(native)

	if test "$CFLAGS" = "" -a "$CXXFLAGS" = ""; then
		CFLAGS="-Wall -O2 -pipe $DEBUG_CFLAGS"
		CXXFLAGS="-Wall -O2 -pipe $DEBUG_CFLAGS"
	fi
	if test "$prefix" = "NONE"; then
		prefix=/usr/local
	fi
	TARGET_PREFIX=$prefix
	if test "$exec_prefix" = "NONE"; then
		exec_prefix=$prefix
	fi
	targetprefix=$prefix
elif test "$TARGET" = "cdk"; then
	AC_MSG_RESULT(cdk)

	if test "$CC" = "" -a "$CXX" = ""; then
		AC_MSG_ERROR([you need to specify variables CC or CXX in cdk])
	fi
	if test "$CFLAGS" = "" -o "$CXXFLAGS" = ""; then
		AC_MSG_ERROR([you need to specify variables CFLAGS and CXXFLAGS in cdk])
	fi
	if test "$prefix" = "NONE"; then
		AC_MSG_ERROR([invalid prefix, you need to specify one in cdk mode])
	fi
	if test "$TARGET_PREFIX" != "NONE"; then
		AC_DEFINE_UNQUOTED(TARGET_PREFIX, "$TARGET_PREFIX",[The targets prefix])
	fi
	if test "$TARGET_PREFIX" = "NONE"; then
		AC_MSG_ERROR([invalid targetprefix, you need to specify one in cdk mode])
		TARGET_PREFIX=""
	fi
else
	AC_MSG_RESULT(none)
	AC_MSG_ERROR([invalid target $TARGET, choose on from native,cdk]);
fi

AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_SYS_LARGEFILE

check_path () {
	return $(perl -e "if(\"$1\"=~m#^/usr/(local/)?bin#){print \"0\"}else{print \"1\";}")
}

])

dnl expand nested ${foo}/bar
AC_DEFUN([TUXBOX_EXPAND_VARIABLE],[__$1="$2"
	for __CNT in false false false false true; do dnl max 5 levels of indirection

		$1=`eval echo "$__$1"`
		echo ${$1} | grep -q '\$' || break # 'grep -q' is POSIX, exit if no $ in variable
		__$1="${$1}"
	done
	$__CNT && AC_MSG_ERROR([can't expand variable $1=$2]) dnl bail out if we did not expand
])

AC_DEFUN([TUXBOX_APPS_DIRECTORY_ONE],[
AC_ARG_WITH($1,[  $6$7 [[PREFIX$4$5]]],[
	_$2=$withval
	if test "$TARGET" = "cdk"; then
		$2=`eval echo "$TARGET_PREFIX$withval"` # no indirection possible IMNSHO
	else
		$2=$withval
	fi
	TARGET_$2=${$2}
],[
	# RFC 1925: "you can always add another level of indirection..."
	TUXBOX_EXPAND_VARIABLE($2,"${$3}$5")
	if test "$TARGET" = "cdk"; then
		TUXBOX_EXPAND_VARIABLE(_$2,"${target$3}$5")
	else
		_$2=${$2}
	fi
	TARGET_$2=$_$2
])

dnl AC_SUBST($2)
AC_DEFINE_UNQUOTED($2,"$_$2",$7)
AC_SUBST(TARGET_$2)
])

AC_DEFUN([TUXBOX_APPS_DIRECTORY],[
AC_REQUIRE([TUXBOX_APPS])

if test "$TARGET" = "cdk"; then
	datadir="\${prefix}/share"
	sysconfdir="\${prefix}/etc"
	localstatedir="\${prefix}/var"
	libdir="\${prefix}/lib"
	mntdir="\${prefix}/mnt"
	targetdatadir="\${TARGET_PREFIX}/share"
	targetsysconfdir="\${TARGET_PREFIX}/etc"
	targetlocalstatedir="\${TARGET_PREFIX}/var"
	targetlibdir="\${TARGET_PREFIX}/lib"
	targetmntdir="\${TARGET_PREFIX}/mnt"
else
	mntdir="/mnt" # hack
fi

TUXBOX_APPS_DIRECTORY_ONE(configdir,CONFIGDIR,localstatedir,/var,/tuxbox/config,
	[--with-configdir=PATH         ],[where to find the config files])

TUXBOX_APPS_DIRECTORY_ONE(datadir,DATADIR,datadir,/share,/tuxbox,
	[--with-datadir=PATH           ],[where to find data])

TUXBOX_APPS_DIRECTORY_ONE(fontdir,FONTDIR,datadir,/share,/fonts,
	[--with-fontdir=PATH           ],[where to find the fonts])

TUXBOX_APPS_DIRECTORY_ONE(gamesdir,GAMESDIR,localstatedir,/var,/tuxbox/games,
	[--with-gamesdir=PATH          ],[where games data is stored])

TUXBOX_APPS_DIRECTORY_ONE(libdir,LIBDIR,libdir,/lib,/tuxbox,
	[--with-libdir=PATH            ],[where to find the internal libs])

TUXBOX_APPS_DIRECTORY_ONE(plugindir,PLUGINDIR,libdir,/lib,/tuxbox/plugins,
	[--with-plugindir=PATH         ],[where to find the plugins])

TUXBOX_APPS_DIRECTORY_ONE(plugindir_var,PLUGINDIR_VAR,localstatedir,/var,/tuxbox/plugins,
	[--with-plugindir_var=PATH     ],[where to find the plugins in /var])

TUXBOX_APPS_DIRECTORY_ONE(webtvdir_var,WEBTVDIR_VAR,localstatedir,/var,/tuxbox/plugins/webtv,
	[--with-webtvdir_var=PATH      ],[where to find the livestreamScriptPath in /var])

TUXBOX_APPS_DIRECTORY_ONE(plugindir_mnt,PLUGINDIR_MNT,mntdir,/mnt,/plugins,
	[--with-plugindir_mnt=PATH     ],[where to find the the extern plugins])

TUXBOX_APPS_DIRECTORY_ONE(luaplugindir,LUAPLUGINDIR,libdir,/lib,/tuxbox/luaplugins,
	[--with-luaplugindir=PATH      ],[where to find Lua plugins])

TUXBOX_APPS_DIRECTORY_ONE(localedir,LOCALEDIR,datadir,/share, /tuxbox/neutrino/locale,
	[--with-localedir=PATH         ],[where to find the locale])

TUXBOX_APPS_DIRECTORY_ONE(localedir_var,LOCALEDIR_VAR,localstatedir,/var,/tuxbox/locale,
	[--with-localedir_var=PATH     ],[where to find the locale in /var])

TUXBOX_APPS_DIRECTORY_ONE(themesdir,THEMESDIR,datadir,/share, /tuxbox/neutrino/themes,
	[--with-themesdir=PATH         ],[where to find the themes])

TUXBOX_APPS_DIRECTORY_ONE(themesdir_var,THEMESDIR_VAR,localstatedir,/var,/tuxbox/themes,
	[--with-themesdir_var=PATH     ],[where to find the themes in /var])

TUXBOX_APPS_DIRECTORY_ONE(iconsdir,ICONSDIR,datadir,/share, /tuxbox/neutrino/icons,
	[--with-iconsdir=PATH          ],[where to find the icons])

TUXBOX_APPS_DIRECTORY_ONE(iconsdir_var,ICONSDIR_VAR,localstatedir,/var,/tuxbox/icons,
	[--with-iconsdir_var=PATH      ],[where to find the icons in /var])

TUXBOX_APPS_DIRECTORY_ONE(private_httpddir,PRIVATE_HTTPDDIR,datadir,/share,/tuxbox/neutrino/httpd,
	[--with-private_httpddir=PATH  ],[where to find the the private httpd files])

TUXBOX_APPS_DIRECTORY_ONE(public_httpddir,PUBLIC_HTTPDDIR,localstatedir,/var,/httpd,
	[--with-public_httpddir=PATH   ],[where to find the the public httpd files])

TUXBOX_APPS_DIRECTORY_ONE(hosted_httpddir,HOSTED_HTTPDDIR,mntdir,/mnt,/hosted,
	[--with-hosted_httpddir=PATH   ],[where to find the the hosted files])
])

dnl automake <= 1.6 needs this specifications
AC_SUBST(CONFIGDIR)
AC_SUBST(DATADIR)
AC_SUBST(FONTDIR)
AC_SUBST(GAMESDIR)
AC_SUBST(LIBDIR)
AC_SUBST(MNTDIR)
AC_SUBST(PLUGINDIR)
AC_SUBST(LUAPLUGINDIR)
AC_SUBST(LOCALEDIR)
AC_SUBST(THEMESDIR)
AC_SUBST(ICONSDIR)
AC_SUBST(PRIVATE_HTTPDDIR)
AC_SUBST(PUBLIC_HTTPDDIR)
AC_SUBST(HOSTED_HTTPDDIR)
dnl end workaround

AC_DEFUN([_TUXBOX_APPS_LIB_CONFIG],[
AC_PATH_PROG($1_CONFIG,$2,no)
if test "$$1_CONFIG" != "no"; then
	if test "$TARGET" = "cdk" && check_path "$$1_CONFIG"; then
		AC_MSG_$3([could not find a suitable version of $2]);
	else
		if test "$1" = "CURL"; then
			$1_CFLAGS=$($$1_CONFIG --cflags)
			$1_LIBS=$($$1_CONFIG --libs)
		else
			if test "$1" = "FREETYPE"; then
				$1_CFLAGS=$($$1_CONFIG --cflags)
				$1_LIBS=$($$1_CONFIG --libs)
			else
				$1_CFLAGS=$($$1_CONFIG --prefix=$TARGET_PREFIX --cflags)
				$1_LIBS=$($$1_CONFIG --prefix=$TARGET_PREFIX --libs)
			fi
		fi
	fi
fi

AC_SUBST($1_CFLAGS)
AC_SUBST($1_LIBS)
])

AC_DEFUN([TUXBOX_APPS_LIB_CONFIG],[
_TUXBOX_APPS_LIB_CONFIG($1,$2,ERROR)
if test "$$1_CONFIG" = "no"; then
	AC_MSG_ERROR([could not find $2]);
fi
])

AC_DEFUN([TUXBOX_APPS_LIB_CONFIG_CHECK],[
_TUXBOX_APPS_LIB_CONFIG($1,$2,WARN)
])

AC_DEFUN([TUXBOX_APPS_PKGCONFIG],[
m4_pattern_forbid([^_?PKG_[A-Z_]+$])
m4_pattern_allow([^PKG_CONFIG(_PATH)?$])
AC_ARG_VAR([PKG_CONFIG], [path to pkg-config utility])dnl
if test "x$ac_cv_env_PKG_CONFIG_set" != "xset"; then
	AC_PATH_TOOL([PKG_CONFIG], [pkg-config])
fi
if test x"$PKG_CONFIG" = x"" ; then
	AC_MSG_ERROR([could not find pkg-config]);
fi
])

AC_DEFUN([_TUXBOX_APPS_LIB_PKGCONFIG],[
AC_REQUIRE([TUXBOX_APPS_PKGCONFIG])
AC_MSG_CHECKING(for package $2)
if $PKG_CONFIG --exists "$2" ; then
	AC_MSG_RESULT(yes)
	$1_CFLAGS=$($PKG_CONFIG --cflags "$2")
	$1_LIBS=$($PKG_CONFIG --libs "$2")
	$1_EXISTS=yes
else
	AC_MSG_RESULT(no)
fi

AC_SUBST($1_CFLAGS)
AC_SUBST($1_LIBS)
])

AC_DEFUN([TUXBOX_APPS_LIB_PKGCONFIG],[
_TUXBOX_APPS_LIB_PKGCONFIG($1,$2)
if test x"$$1_EXISTS" != xyes; then
	AC_MSG_ERROR([could not find package $2]);
fi
])

AC_DEFUN([TUXBOX_APPS_LIB_PKGCONFIG_CHECK],[
_TUXBOX_APPS_LIB_PKGCONFIG($1,$2)
])

AC_DEFUN([TUXBOX_BOXTYPE],[
AC_ARG_WITH(boxtype,
	[  --with-boxtype          valid values: tripledragon,coolstream,spark,azbox,generic,duckbox,spark7162],
	[case "${withval}" in
		tripledragon|coolstream|azbox|generic)
			BOXTYPE="$withval"
			;;
		spark|spark7162)
			BOXTYPE="spark"
			BOXMODEL="$withval"
			;;
		dm*)
			BOXTYPE="dreambox"
			BOXMODEL="$withval"
			;;
		ufs*)
			BOXTYPE="duckbox"
			BOXMODEL="$withval"
			;;
		atevio*)
			BOXTYPE="duckbox"
			BOXMODEL="$withval"
			;;
		fortis*)
			BOXTYPE="duckbox"
			BOXMODEL="$withval"
			;;
		octagon*)
			BOXTYPE="duckbox"
			BOXMODEL="$withval"
			;;
		hs7*)
			BOXTYPE="duckbox"
			BOXMODEL="$withval"
			;;
		dp*)
			BOXTYPE="duckbox"
			BOXMODEL="$withval"
			;;
		cuberevo*)
			BOXTYPE="duckbox"
			BOXMODEL="$withval"
			;;
		ipbox*)
			BOXTYPE="duckbox"
			BOXMODEL="$withval"
			;;
		arivalink200)
			BOXTYPE="duckbox"
			BOXMODEL="$withval"
			;;
		tf*)
			BOXTYPE="duckbox"
			BOXMODEL="$withval"
			;;
		hl101)
			BOXTYPE="duckbox"
			BOXMODEL="$withval"
			;;	
		*)
			AC_MSG_ERROR([bad value $withval for --with-boxtype]) ;;
	esac], [BOXTYPE="coolstream"])

AC_ARG_WITH(boxmodel,
	[  --with-boxmodel         valid for coolstream: hd1, hd2
                          valid for generic: raspi
                          valid for duckbox: ufs910, ufs912, ufs913, ufs922, atevio7500, fortis_hdbox, octagon1008, hs7110, hs7810a, hs7119, hs7819, dp7000, cuberevo, cuberevo_mini, cuberevo_mini2, cuberevo_250hd, cuberevo_2000hd, cuberevo_3000hd, ipbox9900, ipbox99, ipbox55, arivalink200, tf7700, hl101
                          valid for spark: spark, spark7162],
	[case "${withval}" in
		hd1|hd2)
			if test "$BOXTYPE" = "coolstream"; then
				BOXMODEL="$withval"
			else
				AC_MSG_ERROR([unknown model $withval for boxtype $BOXTYPE])
			fi
			;;
		nevis|apollo)
			if test "$BOXTYPE" = "coolstream"; then
				if test "$withval" = "nevis"; then
					BOXMODEL="hd1"
				fi
				if test "$withval" = "apollo"; then
					BOXMODEL="hd2"
				fi
			else
				AC_MSG_ERROR([unknown model $withval for boxtype $BOXTYPE])
			fi
			;;
		ufs910|ufs912|ufs913|ufs922|atevio7500|fortis_hdbox|octagon1008|hs7110|hs7810a|hs7119|hs7819|dp7000|cuberevo|cuberevo_mini|cuberevo_mini2|cuberevo_250hd|cuberevo_2000hd|cuberevo_3000hd|ipbox9900|ipbox99|ipbox55|arivalink200|tf7700|hl101)
			if test "$BOXTYPE" = "duckbox"; then
				BOXMODEL="$withval"
			else
				AC_MSG_ERROR([unknown model $withval for boxtype $BOXTYPE])
			fi
			;;
		spark|spark7162)
			if test "$BOXTYPE" = "spark"; then
				BOXMODEL="$withval"
			else
				AC_MSG_ERROR([unknown model $withval for boxtype $BOXTYPE])
			fi
			;;
		raspi)
			if test "$BOXTYPE" = "generic"; then
				BOXMODEL="$withval"
			else
				AC_MSG_ERROR([unknown model $withval for boxtype $BOXTYPE])
			fi
			;;
		*)
			AC_MSG_ERROR([unsupported value $withval for --with-boxmodel])
			;;
	esac], [test "$BOXTYPE" = "coolstream" && BOXMODEL="hd1" || true]
	)

AC_SUBST(BOXTYPE)
AC_SUBST(BOXMODEL)

AM_CONDITIONAL(BOXTYPE_AZBOX, test "$BOXTYPE" = "azbox")
AM_CONDITIONAL(BOXTYPE_TRIPLE, test "$BOXTYPE" = "tripledragon")
AM_CONDITIONAL(BOXTYPE_COOL, test "$BOXTYPE" = "coolstream")
AM_CONDITIONAL(BOXTYPE_SPARK, test "$BOXTYPE" = "spark")
AM_CONDITIONAL(BOXTYPE_GENERIC, test "$BOXTYPE" = "generic")
AM_CONDITIONAL(BOXTYPE_DUCKBOX, test "$BOXTYPE" = "duckbox")
AM_CONDITIONAL(BOXMODEL_CS_HD1,test "$BOXMODEL" = "hd1")
AM_CONDITIONAL(BOXMODEL_CS_HD2,test "$BOXMODEL" = "hd2")
AM_CONDITIONAL(BOXMODEL_UFS910,test "$BOXMODEL" = "ufs910")
AM_CONDITIONAL(BOXMODEL_UFS912,test "$BOXMODEL" = "ufs912")
AM_CONDITIONAL(BOXMODEL_UFS913,test "$BOXMODEL" = "ufs913")
AM_CONDITIONAL(BOXMODEL_UFS922,test "$BOXMODEL" = "ufs922")
AM_CONDITIONAL(BOXMODEL_SPARK,test "$BOXMODEL" = "spark")
AM_CONDITIONAL(BOXMODEL_SPARK7162,test "$BOXMODEL" = "spark7162")
AM_CONDITIONAL(BOXMODEL_ATEVIO7500,test "$BOXMODEL" = "atevio7500")
AM_CONDITIONAL(BOXMODEL_FORTIS_HDBOX,test "$BOXMODEL" = "fortis_hdbox")
AM_CONDITIONAL(BOXMODEL_OCTAGON1008,test "$BOXMODEL" = "octagon1008")
AM_CONDITIONAL(BOXMODEL_HS7110,test "$BOXMODEL" = "hs7110")
AM_CONDITIONAL(BOXMODEL_HS7810A,test "$BOXMODEL" = "hs7810a")
AM_CONDITIONAL(BOXMODEL_HS7119,test "$BOXMODEL" = "hs7119")
AM_CONDITIONAL(BOXMODEL_HS7819,test "$BOXMODEL" = "hs7819")
AM_CONDITIONAL(BOXMODEL_DP7000,test "$BOXMODEL" = "dp7000")

AM_CONDITIONAL(BOXMODEL_CUBEREVO,test "$BOXMODEL" = "cuberevo")
AM_CONDITIONAL(BOXMODEL_CUBEREVO_MINI,test "$BOXMODEL" = "cuberevo_mini")
AM_CONDITIONAL(BOXMODEL_CUBEREVO_MINI2,test "$BOXMODEL" = "cuberevo_mini2")
AM_CONDITIONAL(BOXMODEL_CUBEREVO_250HD,test "$BOXMODEL" = "cuberevo_250hd")
AM_CONDITIONAL(BOXMODEL_CUBEREVO_2000HD,test "$BOXMODEL" = "cuberevo_2000hd")
AM_CONDITIONAL(BOXMODEL_CUBEREVO_3000HD,test "$BOXMODEL" = "cuberevo_3000hd")
AM_CONDITIONAL(BOXMODEL_IPBOX9900,test "$BOXMODEL" = "ipbox9900")
AM_CONDITIONAL(BOXMODEL_IPBOX99,test "$BOXMODEL" = "ipbox99")
AM_CONDITIONAL(BOXMODEL_IPBOX55,test "$BOXMODEL" = "ipbox55")
AM_CONDITIONAL(BOXMODEL_ARIVALINK200,test "$BOXMODEL" = "arivalink200")
AM_CONDITIONAL(BOXMODEL_TF7700,test "$BOXMODEL" = "tf7700")
AM_CONDITIONAL(BOXMODEL_HL101,test "$BOXMODEL" = "hl101")

AM_CONDITIONAL(BOXMODEL_RASPI,test "$BOXMODEL" = "raspi")

if test "$BOXTYPE" = "azbox"; then
	AC_DEFINE(HAVE_AZBOX_HARDWARE, 1, [building for an azbox])
elif test "$BOXTYPE" = "tripledragon"; then
	AC_DEFINE(HAVE_TRIPLEDRAGON, 1, [building for a tripledragon])
elif test "$BOXTYPE" = "coolstream"; then
	AC_DEFINE(HAVE_COOL_HARDWARE, 1, [building for a coolstream])
elif test "$BOXTYPE" = "spark"; then
	AC_DEFINE(HAVE_SPARK_HARDWARE, 1, [building for a goldenmedia 990 or edision pingulux])
elif test "$BOXTYPE" = "generic"; then
	AC_DEFINE(HAVE_GENERIC_HARDWARE, 1, [building for a generic device like a standard PC])
elif test "$BOXTYPE" = "duckbox"; then
	AC_DEFINE(HAVE_DUCKBOX_HARDWARE, 1, [building for a duckbox])
fi

# TODO: do we need more defines?
if test "$BOXMODEL" = "hd1"; then
	AC_DEFINE(BOXMODEL_CS_HD1, 1, [coolstream hd1/neo/neo2/zee])
elif test "$BOXMODEL" = "hd2"; then
	AC_DEFINE(BOXMODEL_CS_HD2, 1, [coolstream tank/trinity/trinity v2/trinity duo/zee2/link])
elif test "$BOXMODEL" = "ufs910"; then
	AC_DEFINE(BOXMODEL_UFS910, 1, [ufs910])
elif test "$BOXMODEL" = "ufs912"; then
	AC_DEFINE(BOXMODEL_UFS912, 1, [ufs912])
elif test "$BOXMODEL" = "ufs913"; then
	AC_DEFINE(BOXMODEL_UFS913, 1, [ufs913])
elif test "$BOXMODEL" = "ufs922"; then
	AC_DEFINE(BOXMODEL_UFS922, 1, [ufs922])
elif test "$BOXMODEL" = "spark"; then
	AC_DEFINE(BOXMODEL_SPARK, 1, [spark])
elif test "$BOXMODEL" = "spark7162"; then
	AC_DEFINE(BOXMODEL_SPARK7162, 1, [spark7162])
elif test "$BOXMODEL" = "atevio7500"; then
	AC_DEFINE(BOXMODEL_ATEVIO7500, 1, [atevio7500])
elif test "$BOXMODEL" = "fortis_hdbox"; then
	AC_DEFINE(BOXMODEL_FORTIS_HDBOX, 1, [fortis_hdbox])
elif test "$BOXMODEL" = "octagon1008"; then
	AC_DEFINE(BOXMODEL_OCTAGON1008, 1, [octagon1008])
elif test "$BOXMODEL" = "hs7110"; then
	AC_DEFINE(BOXMODEL_HS7110, 1, [hs7110])
elif test "$BOXMODEL" = "hs7810a"; then
	AC_DEFINE(BOXMODEL_HS7810A, 1, [hs7810a])
elif test "$BOXMODEL" = "hs7119"; then
	AC_DEFINE(BOXMODEL_HS7119, 1, [hs7119])
elif test "$BOXMODEL" = "hs7819"; then
	AC_DEFINE(BOXMODEL_HS7819, 1, [hs7819])
elif test "$BOXMODEL" = "dp7000"; then
	AC_DEFINE(BOXMODEL_DP7000, 1, [dp7000])
elif test "$BOXMODEL" = "cuberevo"; then
	AC_DEFINE(BOXMODEL_CUBEREVO, 1, [cuberevo])
elif test "$BOXMODEL" = "cuberevo_mini"; then
	AC_DEFINE(BOXMODEL_CUBEREVO_MINI, 1, [cuberevo_mini])
elif test "$BOXMODEL" = "cuberevo_mini2"; then
	AC_DEFINE(BOXMODEL_CUBEREVO_MINI2, 1, [cuberevo_mini2])
elif test "$BOXMODEL" = "cuberevo_250hd"; then
	AC_DEFINE(BOXMODEL_CUBEREVO_250HD, 1, [cuberevo_250hd])
elif test "$BOXMODEL" = "cuberevo_2000hd"; then
	AC_DEFINE(BOXMODEL_CUBEREVO_2000HD, 1, [cuberevo_2000hd])
elif test "$BOXMODEL" = "cuberevo_3000hd"; then
	AC_DEFINE(BOXMODEL_CUBEREVO_3000HD, 1, [cuberevo_3000hd])
elif test "$BOXMODEL" = "ipbox9900"; then
	AC_DEFINE(BOXMODEL_IPBOX9900, 1, [ipbox9900])
elif test "$BOXMODEL" = "ipbox99"; then
	AC_DEFINE(BOXMODEL_IPBOX99, 1, [ipbox99])
elif test "$BOXMODEL" = "ipbox55"; then
	AC_DEFINE(BOXMODEL_IPBOX55, 1, [ipbox55])
elif test "$BOXMODEL" = "arivalink200"; then
	AC_DEFINE(BOXMODEL_ARIVALINK200, 1, [arivalink200])
elif test "$BOXMODEL" = "tf7700"; then
	AC_DEFINE(BOXMODEL_TF7700, 1, [tf7700])
elif test "$BOXMODEL" = "hl101"; then
	AC_DEFINE(BOXMODEL_HL101, 1, [hl101])	
elif test "$BOXMODEL" = "raspi"; then
	AC_DEFINE(BOXMODEL_RASPI, 1, [Raspberry pi])
fi
])

dnl backward compatiblity
AC_DEFUN([AC_GNU_SOURCE],
[AH_VERBATIM([_GNU_SOURCE],
[/* Enable GNU extensions on systems that have them.  */
#ifndef _GNU_SOURCE
# undef _GNU_SOURCE
#endif])dnl
AC_BEFORE([$0], [AC_COMPILE_IFELSE])dnl
AC_BEFORE([$0], [AC_RUN_IFELSE])dnl
AC_DEFINE([_GNU_SOURCE])
])

AC_DEFUN([AC_PROG_EGREP],
[AC_CACHE_CHECK([for egrep], [ac_cv_prog_egrep],
   [if echo a | (grep -E '(a|b)') >/dev/null 2>&1
    then ac_cv_prog_egrep='grep -E'
    else ac_cv_prog_egrep='egrep'
    fi])
 EGREP=$ac_cv_prog_egrep
 AC_SUBST([EGREP])
])
