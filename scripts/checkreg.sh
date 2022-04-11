#!/bin/bash
# checkreg.sh -- check XLX registration server for active registration and correct IP
# Author: K2IE
#
# Run as you wish from /etc/cron.daily or /etc/crontab
# Dependencies: libxml2-utils

# Set the MY variables as appropriate for your reflector
MYXLX=XLXNNN
MYIP=1.2.3.4
MYEMAILS="some@email.com another@email.com"
MYSENDER=root

XML=`curl -s http://xlxapi.rlx.lu/api.php?do=GetReflectorList | \
   grep $MYXLX -B1 -A7`

SUBJECT="$MYXLX Reg OK"

NOW=`date +%s`
THEN=`echo $XML | xmllint --xpath 'string(/reflector/lastcontact)' -`
LAST=$((NOW-THEN))

if (( LAST > 1800 )); then
   SUBJECT="$MYXLX Reg FAULT"
   TEXT1="$MYXLX registration shows DOWN, please check."
else
   TEXT1="$MYXLX registration shows UP, no action needed."
fi

IP=`echo $XML | xmllint --xpath 'string(/reflector/lastip)' -`

if [ "$IP" = "$MYIP" ]; then
   TEXT2="$MYXLX IP Address is still $IP, no action needed."
else
   SUBJECT="$MYXLX Reg FAULT"
   TEXT2="$MYXLX IP address is now $IP, please check."
fi

TEXT="$TEXT1\n$TEXT2"

echo -e $TEXT | mail -r "$MYSENDER" -s "$SUBJECT" $MYEMAILS
