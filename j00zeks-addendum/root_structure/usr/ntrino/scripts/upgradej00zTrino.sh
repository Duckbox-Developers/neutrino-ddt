#!/bin/sh

errorExit()
{
echo "$1"
echo "$1">/dev/vfd
[ -e /tmp/neutrino-mod-j00zek_sh4.ipk ] && rm -rf /tmp/neutrino-mod-j00zek_sh4.ipk
pkill showiframe
sleep 5
exit 1
}

isPublic=`ls /etc/opkg/*.conf|grep -c 'j00zTrino-OPKG.conf'`
if [ $isPublic -eq 0 ]; then
    if [ -f /tmp/neutrino-mod-j00zek_sh4.ipk ]; then
	opkg install /tmp/neutrino-mod-j00zek_sh4.ipk
	[ $? -gt 0 ] && errorExit "Error installing"
	rm -rf /tmp/neutrino-mod-j00zek_sh4.ipk
    else
	errorExit "DEV is up-to-date"
    fi
else
  #update opkg
  [ -f /usr/share/tuxbox/neutrino/icons/updateOPKG.mpg ] && showiframe /usr/share/tuxbox/neutrino/icons/updateOPKG.mpg & 
  echo "opkg update"
  echo "opkg update">/dev/vfd
  opkg update
  sync
  #test connection
  [ -f /usr/share/tuxbox/neutrino/icons/download.mpg ] && showiframe /usr/share/tuxbox/neutrino/icons/download.mpg & 
  echo "Test Inet..."
  echo "Test Inet...">/dev/vfd
  ping -c 1 github.com 1>/dev/null 2>&1
  if [ $? -gt 0 ]; then
    errorExit "No Inet..."
  fi
  #download
  echo "download"
  echo "download">/dev/vfd
  opkg download neutrino-mod-j00zek
  sync
fi
[ ! -f /tmp/neutrino-mod-j00zek_sh4.ipk ] && errorExit "Error downloading package"
(pkill showiframe;sleep 1;[ -f /usr/share/tuxbox/neutrino/icons/install.mpg ] && showiframe /usr/share/tuxbox/neutrino/icons/install.mpg) & 
echo "Instaling...">/dev/vfd
echo "Instaling...">/dev/vfd
opkg install /tmp/neutrino-mod-j00zek_sh4.ipk
if [ $? -gt 0 ]; then
  errorExit "Error installing" 
fi
[ -e /tmp/neutrino-mod-j00zek_sh4.ipk ] && rm -rf /tmp/neutrino-mod-j00zek_sh4.ipk
echo "Done">/dev/vfd
sync
sleep 1
sync
exit 0 