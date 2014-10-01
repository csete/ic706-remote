gcc -Wall -o audio_client src/audio_client.c src/common.c

gcc -Wall -o audio_server src/audio_server.c src/audio_util.c src/common.c \
            `pkg-config --cflags --libs alsa`

gcc -Wall -o ic706_server src/ic706_server.c src/common.c
gcc -Wall -o ic706_client src/ic706_client.c src/common.c

gcc -Wall -o serial_gateway src/serial_gateway.c src/common.c
