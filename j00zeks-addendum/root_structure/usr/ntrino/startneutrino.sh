#!/bin/sh

# Graterlia OS
# homepage: http://graterlia.xunil.pl
# @j00zek
#
# skrypt uruchomieniowy dla roznego rodzaju Neutrino
# wersja 2016-04-02

. /etc/sysconfig/gfunctions #wczytanie funkcji wspólnych dla skryptów Graterlia
. /var/grun/grcstype #wczytania informacji o rodzaju odbiornika
# zaladowanie informacji o konfiguracji o ile istnieje
if [ -e /etc/sysctl.gos ]; then
  . /etc/sysctl.gos
fi
# initial install when unavailable
if [ ! -e /usr/ntrino/bin/neutrino ];then
  cp -f /usr/ntrino/scripts/upgradej00zTrino.sh /tmp/upgradej00zTrino.sh
  /tmp/upgradej00zTrino.sh
  rm -f /tmp/upgradej00zTrino.sh
fi
# if still unavailable, switch again to openPLI
if [ ! -e /usr/ntrino/bin/neutrino ];then
  echo "Error">/dev/vfd
  sleep 10
  rm -f /usr/bin/startneutrino
  (sleep 1;/etc/init.d/gui restart) &
  exit 0
fi

if [ -e /usr/ntrino/version ]; then
  . /usr/ntrino/version
fi
imagename='Neutrino'
#initial copy
[ -e /var/tuxbox/config/zapit/bouquets.xml ] || cp -f /var/tuxbox/config/initial/bouquets.xml /var/tuxbox/config/zapit/
[ -e /var/tuxbox/config/zapit/services.xml ] || cp -f /var/tuxbox/config/initial/services.xml /var/tuxbox/config/zapit/
[ -e /var/tuxbox/config/zapit/ubouquets.xml ] || cp -f /var/tuxbox/config/initial/ubouquets.xml /var/tuxbox/config/zapit/
[ -e /var/tuxbox/config/zapit/zapit.conf ] || cp -f /var/tuxbox/config/initial/zapit.conf /var/tuxbox/config/zapit/

export LD_LIBRARY_PATH=/usr/ntrino/lib/:$LD_LIBRARY_PATH #tu trzymamy biblioteki specyficzne dla neutrino

# Set input links :)
if [ $rcstype == ADB2850 ] || [ $rcstype == SPARK7162 ]; then
  ln -sf /dev/input/event1 /dev/input/nevis_ir
  ln -sf /dev/input/event0 /dev/input/nevis_fp
else
  ln -sf /dev/input/event0 /dev/input/nevis_ir
  ln -sf /dev/input/event1 /dev/input/nevis_fp
fi
[ -e /.version ] && rm -f /.version #|| ln -sf /usr/ntrino/version /.version #info o wersji wyswietlane w neutrino
[ -e /usr/local/share/tuxbox ] || ln -sf /usr/share/tuxbox/ /usr/local/share/tuxbox
[ -e /var/tuxbox/config/neutrino.conf ] || cp -rf /var/tuxbox/config/initial/neutrino.conf /var/tuxbox/config/
#[ -e /usr/local/bin ] || ln -sf /usr/ntrino/bin /usr/local/bin #w razie czego
#[ -e /etc/cron/hourly/GetWeather ] || ln -sf /etc/ntrino/plugins/Weather/GetWeather /etc/cron/hourly/GetWeather #pogoda na infobarze

/etc/init.d/gbootlogo stop
[ -e /tmp/xupnpd-feeds ] || mkdir -p /tmp/xupnpd-feeds

doStartupActions(){
	[ -z "$NdbgFolder" ] && NdbgFolder='/hdd'
	[ -e $NdbgFolder/neutrino.log ] && mv -f $NdbgFolder/neutrino.log $NdbgFolder/neutrino.log.prev
	#czy logowac?
	if [ -z "$Ndbg" ]; then
		DebugPlace=' -v 0 >>/dev/null 2>&1'
	elif [ "$Ndbg" -eq 0 ]; then
		DebugPlace=' -v 0 >>/dev/null 2>&1'
	else
		DebugPlace=" -v $Ndbg >>$NdbgFolder/neutrino.log 2>&1"
		if [ "$NdbgHal" == "on" ]; then
			HAL_DEBUG=255
			export HAL_DEBUG
		fi
	fi
	
	echo 1 > /proc/sys/vm/drop_caches #czyscimy cache
	stfbcontrol a 255
	echo "starting $imagename->"
	echo "$imagename">/dev/vfd
	echo "0" > /proc/progress
	#epgFolder
	[ -e /var/tuxbox/config/neutrino.conf ] && epgFolder=`grep 'epg_dir='</var/tuxbox/config/neutrino.conf |cut -d '=' -f2`
	[ -z $epgFolder ] && epgFolder=/hdd/epg #default folder
	[ -d $epgFolder ] || mkdir -p $epgFolder
	[ -f /tmp/.reboot ] && rm -f /tmp/.reboot
	[ -f /hdd/neutrino ] && mv -f /hdd/neutrino /usr/ntrino/bin/neutrino
	[ -f /tmp/neutrino ] && mv -f /tmp/neutrino /usr/ntrino/bin/neutrino
	chmod 755 /usr/ntrino/bin/neutrino
} 

until false
do
  pkill xupnpd 2>/dev/null 
  doStartupActions
  (pkill xupnpd;sleep 10; XUPNPDROOTDIR=/share/xupnpd /usr/ntrino/bin/xupnpd) &
  eval "/usr/ntrino/bin/neutrino ${DebugPlace}"
  rtv=$?
  echo "Neutrino ended <- RTV: " $rtv
  echo $rtv >/var/grun/lastrtv 
  #nie wiem, czy to prawda, ale BP twierdzi, ze przeladowanie bpamod poprawia pare rzeczy
  rmmod bpamem
  insmod /lib/modules/bpamem.ko
  if `grep -q 'language=polski'</var/tuxbox/config/neutrino.conf`;then
    isPL=1
  fi 
  case "$rtv" in
    0) echo "$rtv - shutdown"
	case "$vfdsize" in
	  4)[ $isPL ] && echo "STOP">/dev/vfd || echo "off">/dev/vfd;;
	  8)[ $isPL ] && echo "STOP">/dev/vfd || echo "SHUTDOWN">/dev/vfd;;
	  *)[ $isPL ] && echo "Wylaczanie">/dev/vfd || echo "SHUTDOWN">/dev/vfd;;
	esac
	sync
	init 0
	exit 0
	;;
    1) echo "$rtv - REBOOT"
	if [ -f /tmp/.reboot ];then #this file is created by neutrino, so if all correct and no GS
	  case "$vfdsize" in
	    4)[ $isPL ] && echo "REbo">/dev/vfd || echo "REbo">/dev/vfd;;
	    8)[ $isPL ] && echo "Restart.">/dev/vfd || echo "Reboot">/dev/vfd;;
	    *)[ $isPL ] && echo "Restart tunera">/dev/vfd || echo "Reboot">/dev/vfd;;
	  esac
	  sync
	  init 6
	else
	  case "$vfdsize" in
	    4)[ $isPL ] && echo "StoP">/dev/vfd || echo "StoP">/dev/vfd;;
	    8)[ $isPL ] && echo "Blad Krytyczny">/dev/vfd || echo "Fatal error">/dev/vfd;;
	    *)[ $isPL ] && echo "Blad krytyczny">/dev/vfd || echo "Fatal error">/dev/vfd;;
	  esac
	fi
	exit 0
	;;
    2) echo "$rtv - openPLI"
	hasOpenPLI=`ls /usr/bin/enigma2* 2>/dev/null|grep -c "."`
	if [ $hasOpenPLI -gt 0 ]; then
	  case "$vfdsize" in
	    4)[ $isPL ] && echo "oPLi">/dev/vfd || echo "oPLi">/dev/vfd;;
	    8)[ $isPL ] && echo "openPLi">/dev/vfd || echo "openPLi">/dev/vfd;;
	    *)[ $isPL ] && echo "Start openPLi">/dev/vfd || echo "Start openPLi">/dev/vfd;;
	  esac
	  sync
	  rm -f /usr/bin/startneutrino
	  (sleep 1;/etc/init.d/gui restart) &
	  exit 0
	fi
	;;
    3) echo "$rtv - update"
	cp -f /usr/ntrino/scripts/upgradej00zTrino.sh /tmp/upgradej00zTrino.sh
	chmod 755 /tmp/upgradej00zTrino.sh
	/tmp/upgradej00zTrino.sh
	rm -f /tmp/upgradej00zTrino.sh
	pkill showiframe
	(sync;sleep 2;/etc/init.d/gui restart) &
	exit 0
	;; 
    139|127) echo "* - ERROR $rtv"
	case "$vfdsize" in
	  4)[ $isPL ] && echo "b $rtv">/dev/vfd || echo "E $rtv">/dev/vfd;;
	  8)[ $isPL ] && echo "Blad $rtv">/dev/vfd || echo "Err $rtv">/dev/vfd;;
	  *)[ $isPL ] && echo "Blad $rtv">/dev/vfd || echo "Error $rtv">/dev/vfd;;
	esac
	exit 0
	;;
    *) echo "* - ERROR $rtv"
	case "$vfdsize" in
	  4)[ $isPL ] && echo "b $rtv">/dev/vfd || echo "E $rtv">/dev/vfd;;
	  8)[ $isPL ] && echo "Blad $rtv">/dev/vfd || echo "Err $rtv">/dev/vfd;;
	  *)[ $isPL ] && echo "STOP-Blad $rtv">/dev/vfd || echo "STOP-Error $rtv">/dev/vfd;;
	esac
	sleep 1
	;;
  esac
done  