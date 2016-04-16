#!/bin/sh

removeTarGz(){
[ -e /tmp/j00zTrino.tar.gz ] && rm -rf /tmp/j00zTrino.tar.gz
}

[ -e /usr/ntrino/.debug ] && rm -f /usr/ntrino/.debug
removeTarGz
curl --help 1>/dev/null 2>&1
if [ $? -gt 0 ]; then
  echo "_(Required program 'curl' is not installed. Trying to install it via OPKG.)"
  echo "curl...">/dev/vfd
  echo
  opkg install curl 

  curl --help 1>/dev/null 2>&1
  if [ $? -gt 0 ]; then
    echo
    echo "_(Required program 'curl' is not available. Please install it first manually.)"
    exit 1
  fi
fi

echo "_(Checking internet connection...)"
echo "InEt...">/dev/vfd
ping -c 1 github.com 1>/dev/null 2>&1
if [ $? -gt 0 ]; then
  echo "_(github server unavailable, update impossible!!!)"
  exit 1
fi

echo "_(Downloading latest j00zTrino version...)"
echo "download">/dev/vfd
curl -kLs https://github.com/j00zek/eePlugins/blob/master/j00zTrino/neutrino-mp-cst.tar.gz?raw=true -o /tmp/j00zTrino.tar.gz
if [ $? -gt 0 ]; then
  echo "_(Archive downloaded improperly"
  removeTarGz
  exit 1
fi

if [ ! -e /tmp/j00zTrino.tar.gz ]; then
  echo "_(No archive downloaded, check your curl version)"
  exit 1
fi

echo "_(Unpacking new version...)"
echo "InStAl..">/dev/vfd
#cd /tmp
tar -zxf /tmp/j00zTrino.tar.gz -C /
if [ $? -gt 0 ]; then
  echo "_(Archive unpacked improperly)"
  removeTarGz
  exit 1
fi

[ -e /usr/ntrino/.debug ] || touch /usr/ntrino/.debug
removeTarGz
exit 0 