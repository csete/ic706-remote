#!/bin/bash
#
# Simple application manager used to start and stop the client application
# in repsonse to hardware inputs (GPIO 67 and 68)
#

echo "Starting application manager"

# GPIO for activating control client
if [ ! -e /sys/class/gpio/gpio67 ]
then
    echo "67" > /sys/class/gpio/export
fi
echo "in" > /sys/class/gpio/gpio67/direction
echo "1"  > /sys/class/gpio/gpio67/active_low
# edge doesn't matter if we are not using poll or select


# GPIO for activating audio client
if [ ! -e /sys/class/gpio/gpio68 ]
then
    echo "68" > /sys/class/gpio/export
fi
echo "in" > /sys/class/gpio/gpio68/direction
echo "1"  > /sys/class/gpio/gpio68/active_low


while true
do
    CTLON=`systemctl is-active ic706-client.service`
    GPIO67=`cat /sys/class/gpio/gpio67/value`

    if [ "$GPIO67" -eq "1" ]
    then
        if [ "$CTLON" == "inactive" ]
        then
            echo "Launching IC-706 control client..."
            systemctl start ic706-client.service
        fi
    else
	if [ "$CTLON" == "active" ]
	then
            echo "Stopping IC-706 control client..."
	    systemctl stop ic706-client.service
	fi
    fi

    AUDON=`systemctl is-active audio-client.service`
    GPIO68=`cat /sys/class/gpio/gpio68/value`

    if [ "$GPIO68" -eq "1" ]
    then
        if [ "$AUDON" == "inactive" ]
        then
            echo "Launching audio client..."
            systemctl start audio-client.service
        fi
    else
	if [ "$AUDON" == "active" ]
	then
            echo "Stopping audio client..."
	    systemctl stop audio-client.service
	fi
    fi

    sleep 1
done

echo "Application manager stopped"

