#!/bin/bash
#
# Play opus encoded audio stream from a server
#
usage ()
{
    echo "Usage: audio_client.sh <audiodev> <host> <port>"
}

if [ $# -ne 3 ]; then
    usage
    exit 0
fi

echo "Playing audio stream from $2:$3"
echo "Audio device: $1"

while true; do
    gst-launch-1.0 -v -e tcpclientsrc host=$2 port=$3 ! \
        application/x-rtp, media=audio,clock-rate=48000,payload=96 ! \
        rtpopusdepay ! opusdec ! alsasink device=$1 sync=false

    echo "Wait for 5 seconds..."
    sleep 5
done

