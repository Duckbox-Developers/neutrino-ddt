#!/bin/sh

# Graterlia OS
# homepage: http://graterlia.xunil.pl
# e-mail: nbox@xunil.pl
#
# skrypt uczyszcz¹cy linki zrobione dla Neutrino
# wersja 2015-03-19

. /etc/sysconfig/gfunctions #wczytanie funkcji wspólnych dla skryptów Graterlia
# za³adowanie informacji o konfiguracji o ile istnieje
if [ -e /etc/sysctl.gos ]; then
	. /etc/sysctl.gos
fi

[ -e /dev/input/nevis_ir ] && rm -rf /dev/input/nevis_ir #obsluga pilota w neutrino
[ -e /etc/cron/hourly/GetWeather ] && rm -rf /etc/cron/hourly/GetWeather #pogoda na infobarze
#[ -e /usr/local/bin ] && rm -rf /usr/local/bin #w razie czego
[ -e /.version ] && rm -rf /.version #info o wersji wyswietlane w neutrino
[ -e /usr/local/share/tuxbox ] && rm -rf /usr/local/share/tuxbox
[ -e /var/tuxbox ] && rm -rf /var/tuxbox
[ -e /usr/ntrino ] && rm -rf /usr/ntrino
