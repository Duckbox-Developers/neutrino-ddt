#!/bin/sh

#
# @j00zek dla Graterlia
# e-mail: nbox@xunil.pl
#
# skrypt uczyszczacy linki zrobione dla Neutrino
# wersja 2016-04-17

[ -e /dev/input/nevis_ir ] && rm -rf /dev/input/nevis_ir #obsluga pilota w neutrino
[ -e /etc/cron/hourly/GetWeather ] && rm -rf /etc/cron/hourly/GetWeather #pogoda na infobarze
#[ -e /usr/local/bin ] && rm -rf /usr/local/bin #w razie czego
[ -e /.version ] && rm -rf /.version #info o wersji wyswietlane w neutrino
[ -e /usr/local/share/tuxbox ] && rm -rf /usr/local/share/tuxbox
[ -e /var/tuxbox ] && rm -rf /var/tuxbox
[ -e /usr/ntrino ] && rm -rf /usr/ntrino
[ -e /usr/share/tuxbox ] && rm -rf /usr/share/tuxbox
[ -e /usr/share/lua ] && rm -rf /usr/share/lua
[ -e /usr/share/xupnpd ] && rm -rf /usr/share/xupnpd

exit 0