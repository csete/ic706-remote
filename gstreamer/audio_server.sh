#!/bin/bash
#
# Streams opus encoded audio to a client using a TCP connection
#
usage ()
{
    echo "Usage: audio_server.sh <audiodev> <port>"
}

if [ $# -ne 2 ]; then
    usage
    exit 0
fi

# Gets the IP address after lo
host=`ip addr | grep 'state UP' -A2 | tail -n1 | awk '{print $2}' | cut -f1 -d'/'`

echo "Starting audio streaming server at $host:$2"
echo "Audio device: $1"

while true; do
    gst-launch-1.0 -v alsasrc device=$1 ! \
        audio/x-raw, format=\(string\)S16LE, channels=2, rate=48000 ! \
        opusenc bandwidth=1102 bitrate=24000 audio=true frame-size=60 complexity=3 ! \
        rtpopuspay ! tcpserversink host=$host port=$2

    echo "Sleeping for 5 seconds..."
    sleep 5
done

