#!/bin/sh
# -----------------------------------------------------------
# Live (yjogol)
# $Date$
# $Revision$
# -----------------------------------------------------------

. ./_Y_Globals.sh
. ./_Y_Library.sh

# -----------------------------------------------------------
live_lock()
{
	call_webserver "control/rc?lock" >/dev/null
	call_webserver "control/zapto?stopplayback" >/dev/null
}
# -----------------------------------------------------------
live_unlock()
{
	call_webserver "control/rc?unlock"  >/dev/null
	call_webserver "control/zapto?startplayback" >/dev/null
}
# -----------------------------------------------------------

# -----------------------------------
# Main
# -----------------------------------
# echo "$1" >/tmp/debug.txt
echo "$*"
case "$1" in
	zapto)
		if [ "$2" != "" ]
		then
			call_webserver "control/zapto?$2" >/dev/null
		fi
		;;

	switchto)
		if [ "$2" = "Radio" ]
		then
			call_webserver "control/setmode?radio" >/dev/null
		else
			call_webserver "control/setmode?tv" >/dev/null
		fi
		;;

	url)
		url=`buildStreamingRawURL`
		echo "$url" ;;

	audio-url)
		url=`buildStreamingAudioRawURL`
		echo "$url" ;;

	live_lock)
		live_lock ;;

	live_unlock)
		live_unlock ;;

	dboxIP)
		buildLocalIP ;;

	udp_stream)
		if [ "$2" = "start" ]
		then
			shift 2
			killall streamts
			killall streampes
			killall udpstreamts
			if [ -e $y_path_varbin/udpstreamts ]
			then
				$y_path_varbin/udpstreamts $* &
			else
				udpstreamts $* &
			fi
			pidof udpstreamts >/tmp/udpstreamts.pid
		fi
		if [ "$2" = "stop" ]
		then
			killall udpstreamts
		fi
		;;

	*)
		echo "Parameter wrong: $*" ;;
esac



