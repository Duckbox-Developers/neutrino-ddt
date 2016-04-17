#!/bin/bash
####################################### ENV config #######################################
myroot=/DuckboxDisk/github
cdkpath=$myroot/cdk
j00zTrinoGITpath=$myroot/j00zeks-neutrino-mp-cst-next
j00zTrinoExtras=$j00zTrinoGITpath/j00zeks-addendum
scriptPath=`dirname $0`

fullCompilation=0
###################################### syncing GITs ######################################
tput setaf 3;echo "Syncing GITs...";tput sgr0
cd $cdkpath
if `git fetch -v --dry-run 2>&1| grep -q -v '[up to date]'`;then
  git checkout -f >/dev/null
  git pull
  tput setaf 2;echo "patching neutrino.mk...";tput sgr0
  patch -p1 -i $j00zTrinoExtras/compilation/neutrino.mk.patch
  [ $? -gt 0 ] && exit 1
  if ! `grep -q 'neutrino-owned-by-user' < $cdkpath/make/neutrino.mk`;then
    exit 1
  fi
  fullCompilation=1
fi
cd $myroot/driver;git pull | grep -q -v 'Already up-to-date.' && fullCompilation=1
cd $myroot/apps;git pull | grep -q -v 'Already up-to-date.' && fullCompilation=1

cd $j00zTrinoGITpath
if `git fetch -v --dry-run 2>&1| grep -q -v '[up to date]'`;then
  git pull
  [ $? -gt 0 ] && exit 1
fi
################################# set compilation options ################################
#
# j00zek: we use cuberevo as a base. In fact doesn't matter-j00ztrino works on any E2-SH4
# do NOT compile as spark!!! Its configuration has unwanted changes (e.g. in vfd)
# finally we run full compilation, only when necessary. If not just quick neutrino ;)
#
cd $cdkpath
if `grep -q -v '\-\-enable\-p0217' < $cdkpath/lastChoice`;then
  fullCompilation=1
elif `grep -q -v '\-\-enable\-neutrino' < $cdkpath/lastChoice`;then
  fullCompilation=1
elif `grep -q -v '\-\-with\-boxtype=cuberevo' < $cdkpath/lastChoice`;then
  fullCompilation=1
fi
if [ $fullCompilation -eq 1 ];then
  make clean
  ./make.sh 9 5 n 2 3 1 3
else
  rm -f .deps/neutrino-mp-cst-next*
  rm -f .deps/libstb-hal-cst-next*
fi
####################################### Compilation ######################################
make yaud-neutrino-mp-cst-next
if [ $? -gt 0 ]; then
  tput setaf 1;echo "Compilation ERROR!!! :(";tput sgr0
  exit 1
fi
###################################### Prep package ######################################
#$j00zTrinoExtras
myName='Neutrino mod j00zek'
j00zTrinoRoot=$myroot/tufsbox/j00zTrino
[ -e $j00zTrinoRoot ] && rm -rf $j00zTrinoRoot
[ -e $myroot/tufsbox/neutrino-mp-cst-next.tar.gz ] && rm -f $myroot/tufsbox/neutrino-mp-cst-next.tar.gz
rm -f $myroot/tufsbox/neutrino-mod-j00zek_*sh4.ipk 2/dev/null
tput setaf 3;echo "!!!!!!!!!!!!!!!!!!!!! Prep package !!!!!!!!!!!!!!!!!!!!!";tput sgr0
cd $myroot/tufsbox/release
mkdir -p $j00zTrinoRoot
mkdir -p $j00zTrinoRoot/usr/ntrino
mkdir -p $j00zTrinoRoot/usr/ntrino/lib
mkdir -p $j00zTrinoRoot/usr/ntrino/bin
#kopiowanie binarek
cp -rf ./usr/local/bin/* $j00zTrinoRoot/usr/ntrino/bin
#kopiowanie potrzebnych libow (niektóre są redundantne, ale za to pozwalają odinstalowac openPLI :)
cp -rf ./usr/lib/libass* $j00zTrinoRoot/usr/ntrino/lib/
cp -rf ./usr/lib/libasound* $j00zTrinoRoot/usr/ntrino/lib/
cp -rf ./usr/lib/libavcodec.* $j00zTrinoRoot/usr/ntrino/lib/
cp -rf ./usr/lib/libavformat.* $j00zTrinoRoot/usr/ntrino/lib/
cp -rf ./usr/lib/libavutil.* $j00zTrinoRoot/usr/ntrino/lib/
cp -rf ./usr/lib/libcurl.* $j00zTrinoRoot/usr/ntrino/lib/
cp -rf ./usr/lib/libdvbsi* $j00zTrinoRoot/usr/ntrino/lib/
cp -rf ./usr/lib/libfreetype* $j00zTrinoRoot/usr/ntrino/lib/
cp -rf ./usr/lib/libgif.so* $j00zTrinoRoot/usr/ntrino/lib/
cp -rf ./usr/lib/libjpeg* $j00zTrinoRoot/usr/ntrino/lib/
cp -rf ./usr/lib/liblua* $j00zTrinoRoot/usr/ntrino/lib/ 2>/dev/null
cp -rf ./usr/lib/libOpenThreads.* $j00zTrinoRoot/usr/ntrino/lib/
cp -rf ./usr/lib/libpng16* $j00zTrinoRoot/usr/ntrino/lib/
cp -rf ./usr/lib/librtmp* $j00zTrinoRoot/usr/ntrino/lib/
cp -rf ./usr/lib/libsigc-2.0.* $j00zTrinoRoot/usr/ntrino/lib/
cp -rf ./usr/lib/libswresample.* $j00zTrinoRoot/usr/ntrino/lib/
cp -rf ./usr/lib/libxml2* $j00zTrinoRoot/usr/ntrino/lib/
cp -rf $j00zTrinoExtras/root_structure/*  $j00zTrinoRoot/

echo "imagename=Neutrino mod j00zek">$j00zTrinoRoot/usr/ntrino/version
echo "homepage=http://openpli.xunil.pl">>$j00zTrinoRoot/usr/ntrino/version
echo "creator=j00zek,herpoi">>$j00zTrinoRoot/usr/ntrino/version
echo "docs=http://openpli.xunil.pl/wiki/">>$j00zTrinoRoot/usr/ntrino/version
echo "forum=http://forum.xunil.pl">>$j00zTrinoRoot/usr/ntrino/version
echo "version=`date +"%Y%m%d"`">>$j00zTrinoRoot/usr/ntrino/version
echo "git=Who knows ;)">>$j00zTrinoRoot/usr/ntrino/version
