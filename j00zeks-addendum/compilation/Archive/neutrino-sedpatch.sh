echo "just to simplify setting of default configuration I am lazy"
myroot=/DuckboxDisk/github/eePlugins/j00zTrino/patches
srcroot=/DuckboxDisk/github/source/neutrino-mp-cst-next

# patching neutrino.cpp
sed -i "s/\(^.*g_settings\.lcd_scroll[ \t]*= configfile\.getInt32. \"lcd_scroll\", \)1/\10/" $srcroot/src/neutrino.cpp
sed -i "s/\(^.*g_settings.epg_dir[ \t]*= \)/\1/" $srcroot/src/neutrino.cpp
sed -i "s;/media/sda1/;/hdd/;g" $srcroot/src/neutrino.cpp
sed -i "s/\(^.*g_settings.network_ntpenable[ \t]*= configfile.getBool.*\)false/\1true/" $srcroot/src/neutrino.cpp
sed -i "s;/media/sda1/epg;/hdd/epg;g" $srcroot/src/neutrino.cpp
sed -i 's;\("channellist_show_infobox"[ ]*,[ ]*\)1;\1 0;' $srcroot/src/neutrino.cpp
sed -i 's;\(g_settings.channellist_foot.*"channellist_foot"[ ]*,[ ]*\)1;\1 2;' $srcroot/src/neutrino.cpp
sed -i 's;g_settings.language = "deutsch";g_settings.language = "english";' $srcroot/src/neutrino.cpp
sed -i 's;\(g_settings.channellist_foot.*"channellist_foot"[ ]*,[ ]*\)1;\1 2;' $srcroot/src/neutrino.cpp
sed -i 's;\(g_settings.fan_speed.*"fan_speed"[ ]*,[ ]*\)[0-9];\1 0;' $srcroot/src/neutrino.cpp
sed -i 's;\(g_settings.audio_volume_percent_pcm.*"audio_volume_percent_pcm"[ ]*,[ ]*\)[0-9][0-9]*;\1 65;' $srcroot/src/neutrino.cpp
sed -i 's;\(g_settings.audio_volume_percent_ac3.*"audio_volume_percent_ac3"[ ]*,[ ]*\)[0-9][0-9]*;\1 100;' $srcroot/src/neutrino.cpp
#sed -i 's;\(g_settings.lcd_vfd_scroll.*"lcd_vfd_scroll"[ ]*,[ ]*\)[0-9][0-9]*;\1 0;' $srcroot/src/neutrino.cpp
# patching files in ./src
# patching files in ./src/gui
sed -i "s;/.version;/usr/ntrino/version/;g" $srcroot/src/gui/*.cpp
sed -i "s;/var/lib/opkg;/var/opkg;g" $srcroot/src/gui/opkg_manager.cpp
sed -i "s://set package feeds:return; //j00zek dont touch opkg config file:g" $srcroot/src/gui/opkg_manager.cpp
# patching files in ./src/driver

sed -i 's;"/bin/grab";"/usr/bin/grab";g' $srcroot/src/driver/*.cpp

# patching web server files
sed -i "s;/media/sda1/;/hdd/;g" $srcroot/src/nhttpd/tuxboxapi/controlapi.cpp

sed -i "s/\(caps.can_cec =\) 1/\1 0/" $srcroot/lib/libcoolstream/hardware_caps.cpp
#tlumaczenia
echo "j00zek.openpli Start openPLI">>$srcroot/data/locale/english.locale
echo "j00zek.update Update Neutrino">>$srcroot/data/locale/english.locale
echo "j00zek.restart SoftCam restart">>$srcroot/data/locale/english.locale
echo "j00zek.vfd_show_number number">>$srcroot/data/locale/english.locale
echo "recording.failed Recording failed!">>$srcroot/data/locale/english.locale
#usuniecie bledu pobierania daty
sed -i 's/\(find_executable.*\)"-s "/\1" -s "/' $srcroot/src/eitd/sectionsd.cpp

#temporary
cp -f $myroot/GOS-vfd.cpp $srcroot/src/driver/vfd.cpp

sleep 10
