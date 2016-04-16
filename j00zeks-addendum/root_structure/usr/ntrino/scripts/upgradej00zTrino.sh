#!/bin/sh

removeTarGz(){
[ -e /tmp/j00zTrino.tar.gz ] && rm -rf /tmp/j00zTrino.tar.gz
}

errorExit()
{
echo "$1">/dev/vfd
removeTarGz
pkill showiframe
sleep 5
exit 1
}

#upgrade clean - from old, unnecessary stuff
[ -e /usr/ntrino/.debug ] && rm -f /usr/ntrino/.debug
if ! `grep -q 'Ndbg='</etc/sysctl.gos`;then
  echo "Ndbg=3">>//etc/sysctl.gos
fi
removeTarGz
tuxbox="/var/tuxbox"
Nconfig="$tuxbox/config"
LISTA="$Nconfig/*.off $Nconfig.on $tuxbox/plugins"

for plikORfolder in  $LISTA ; do
	[ -e $plikORfolder ] && rm -fr $plikORfolder || echo "Brak $plikORfolder :)"
done
#upgrade

if [ -f /tmp/neutrino-mp-cst-dev.tar.gz ]; then
  mv -f /tmp/neutrino-mp-cst-dev.tar.gz /tmp/j00zTrino.tar.gz
  sync
  sleep 1
else
  [ -f /usr/share/tuxbox/neutrino/icons/download.mpg ] && showiframe /usr/share/tuxbox/neutrino/icons/download.mpg & 
  sync
  sleep 1
  echo "Checking internet connection..."
  echo "InEt...">/dev/vfd
  ping -c 1 github.com 1>/dev/null 2>&1
  if [ $? -gt 0 ]; then
    echo "github server unavailable, update impossible!!!"
    exit 1
  fi

  echo "Downloading latest j00zTrino version..."
  echo "download">/dev/vfd
  #curl -kLs https://github.com/j00zek/eePlugins/blob/master/j00zTrino/neutrino-mp-cst.tar.gz?raw=true -o /tmp/j00zTrino.tar.gz
  wget -q http://hybrid.xunil.pl/j00ztrino/j00ztrino/neutrino-mp-cst.tar.gz -O /tmp/j00zTrino.tar.gz
  if [ $? -gt 0 ]; then
    errorExit "Error downloading" 
  fi

  if [ ! -e /tmp/j00zTrino.tar.gz ]; then
    errorExit "Error downloading" 
  fi
fi
(pkill showiframe;sleep 1;[ -f /usr/share/tuxbox/neutrino/icons/install.mpg ] && showiframe /usr/share/tuxbox/neutrino/icons/install.mpg) & 
echo "Unpacking new version..."
echo "Instaling...">/dev/vfd
#cd /tmp
tar -zxf /tmp/j00zTrino.tar.gz -C /
if [ $? -gt 0 ]; then
  errorExit "Error installing" 
fi

echo "donE">/dev/vfd
removeTarGz
sync
sleep 1
sync
exit 0 