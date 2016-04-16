#Do NOT touch this file !!!!#
# add your script to one of /var/tuxbox/config/Autosctipts/subfolder> instead
# run-part rune scripts in a sequence in separate thread, to not block neutrino

RunFolder=`basename $0`
[ -d /var/tuxbox/config/AutoScripts/$RunFolder ] && (/bin/run-parts /var/tuxbox/config/AutoScripts/$RunFolder) &
exit 0