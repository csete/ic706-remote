
Following pipelines can be used to stream audio over UDP or TCP connection.


Raw audio over TCP:

gst-launch-1.0 -v alsasrc device="hw:3" ! audio/x-raw, format=\(string\)S16LE, channels=1, rate=48000 ! tcpserversink host=192.168.1.100 port=43000

gst-launch-1.0 -v tcpclientsrc host=192.168.1.100 port=43000 ! audio/x-raw, format=\(string\)S16LE, channels=1, rate=48000 ! alsasink device-name="Generic"



Opus encoded audio over RTP/UDP or RTP/TCP:
(can't make TCP work without RTP)

http://cgit.freedesktop.org/gstreamer/gst-plugins-good/tree/gst/rtp/README

gst-launch-1.0 -v alsasrc device=hw:3 ! audio/x-raw, format=\(string\)S16LE, channels=1, rate=48000 ! opusenc bandwidth=1102 bitrate=12000 audio=true complexity=5 ! rtpopuspay ! udpsink host=localhost port=43000

gst-launch-1.0 -v udpsrc port=43000  ! application/x-rtp, media=audio,clock-rate=48000,payload=96,encoding-name=X-GST-OPUS-DRAFT-SPITTKA-00 ! rtpopusdepay ! opusdec ! alsasink device=hw:1


gst-launch-1.0 -v alsasrc device=hw:3 ! audio/x-raw, format=\(string\)S16LE, channels=1, rate=48000 ! opusenc bandwidth=1102 bitrate=12000 audio=true complexity=5 ! rtpopuspay ! tcpserversink host=localhost port=43000

gst-launch-1.0 -v tcpclientsrc host=localhost port=43000 ! application/x-rtp, media=audio, clock-rate=48000,payload=96 ! rtpopusdepay ! opusdec ! alsasink device=hw:1
