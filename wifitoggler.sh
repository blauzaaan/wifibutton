#!/bin/sh

#-----------------------------------------------------------------------
#
#	Copyright 2017-2021 by blauzaaan, https://github.com/blauzaaan
#
#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
#-----------------------------------------------------------------------

#-----------------------------------------------------------------------
# This is the server-side part of wifitoggler designed for OpenWRT
# Simple and not very secure 
# Adjust LISTEN_PORT and SECRET to your needs
# run:
# /path/to/wifitoggler.sh
#
# control:
# send SECRET,q for querying wifi status.
#      Returns SECRET,ON,A/B or SECRET,OFF,A/B
#      where ON = wifi active, OFF = wifi inactive
#            A = number of active wifi devices
#            B = number of wifi devices total
#
# send SECRET,t for toggling wifi
#      Returns status as above
#
# requires socat to be installed
# daemonizes itself while still spitting out status information to STDERR
#-----------------------------------------------------------------------

LISTEN_PORT=5015

WIFI_DEVICES="@wifi-iface[0] @wifi-iface[1] guest_iface_0 guest_iface_1"

SECRET=SECRET
QUERY_STATUS=$SECRET,q
TOGGLE_COMMAND=$SECRET,t
STATUS_ON=$SECRET,ON
STATUS_OFF=$SECRET,OFF

NUM_DEVICES=0
ACTIVE_DEVICES=0

get_wifi_status() {
	NUM_DEVICES=0
	ACTIVE_DEVICES=0
	for i in $WIFI_DEVICES
	do
		let NUM_DEVICES+=1
		if [ "$(uci get wireless.$i.disabled 2>/dev/null)" = "1" ]
		then
			#wifi on this device is disabled
			echo $i: disabled >&2
		else
			#wifi on this device is enabled
			echo $i: enabled >&2
			let ACTIVE_DEVICES++
		fi
	done

#	if [ "$(uci get wireless.${WIFI_DEVICE}.disabled 2>/dev/null)" = "1" ]
#	then
#		#wifi is disabled
#		disabled=1
#	else
#		#wifi is enabled
#		disabled=0
#	fi
}

send_wifi_status() {
	get_wifi_status
	status=""

	if [ "$ACTIVE_DEVICES" -gt "0" ]
	then
		status=$STATUS_ON
	else
		status=$STATUS_OFF
	fi
	status="$status,$ACTIVE_DEVICES/$NUM_DEVICES"
	echo answering: $status >&2
	echo $status
}

toggle_wifi() {
	get_wifi_status
	if [ "$ACTIVE_DEVICES" -gt "0" ]
	then
		#disable all
		disabled=1
	else
		#enable all
		disabled=0
	fi

	for i in $WIFI_DEVICES
        do
		uci set "wireless.$i.disabled=$disabled"
	done


#	if [ $disabled = 1 ]
#	then
#		disabled=0
#	else
#		disabled=1
#	fi
	
#	uci set "wireless.${WIFI_DEVICE}.disabled=$disabled"
	
	if ubus list network.wireless >/dev/null 2>&1; then
			ubus call network reload
	else
			wifi
	fi

	uci commit
}

start_server() {
	echo starting >&2
	while read line
	do
		line=`echo -n $line|tr -d '\0\r\n'`
		case $line in
		$QUERY_STATUS)
			echo got query status message $QUERY_STATUS >&2
			send_wifi_status
			;;
		$TOGGLE_COMMAND)
			echo got toggle command: $TOGGLE_COMMAND >&2
			toggle_wifi
			send_wifi_status
			;;
		*)
			echo unkown command: $line >&2
			;;
		esac
	done
	echo exiting >&2
}

if [ "$1" = "-s" ]
then
#	start_server
	start_server 2>/dev/null
#	start_server 2>/root/wifitoggler-logs/wifitoggler-$$.log
else
	socat tcp-listen:$LISTEN_PORT,reuseaddr,fork,keepalive,keepidle=10,keepintvl=10,keepcnt=2 EXEC:"$0 -s" &
fi
