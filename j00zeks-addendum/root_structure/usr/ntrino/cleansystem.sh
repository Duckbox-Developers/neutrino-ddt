#!/bin/sh

#
# @j00zek dla Graterlia
#
# skrypt uczyszczacy linki zrobione dla Neutrino
# wersja 2016-04-18

[ -e /dev/input/nevis_ir ] && rm -rf /dev/input/nevis_ir #obsluga pilota w neutrino
[ -e /dev/input/nevis_fp ] && rm -rf /dev/input/nevis_fp #obsluga przyciskow panela
[ -e /etc/cron/hourly/GetWeather ] && rm -rf /etc/cron/hourly/GetWeather #pogoda na infobarze
[ -e /.version ] && rm -rf /.version #info o wersji wyswietlane w neutrino
[ -d /hdd/epg ] rm -rf /hdd/epg
[ -e /usr/local/share/tuxbox ] && rm -rf /usr/local/share/tuxbox
[ -e /var/tuxbox ] && rm -rf /var/tuxbox
[ -e /usr/ntrino ] && rm -rf /usr/ntrino
[ -e /usr/share/lua ] && rm -rf /usr/share/lua
[ -e /usr/share/neutrino ] && rm -rf /usr/share/neutrino
[ -e /usr/share/tuxbox ] && rm -rf /usr/share/tuxbox
[ -e /usr/share/xupnpd ] && rm -rf /usr/share/xupnpd

exit 0