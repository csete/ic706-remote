/*
 * Copyright (c) 2014, Alexandru Csete
 * All rights reserved.
 *
 * This software is licensed under the terms and conditions of the
 * Simplified BSD License. See license.txt for details.
 *
 */
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>           // PRId64 and PRIu64
#include <netinet/in.h>
#include <opus.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "audio_util.h"
#include "common.h"

/* application state and config */
struct app_data {
    uint32_t        sample_rate;        /* audio sample rate */
    int             device_index;       /* audio device index */
    int             server_port;        /* network port number */
    char           *server_ip;
};

static int      keep_running = 1;       /* set to 0 to exit infinite loop */

void signal_handler(int signo)
{
    fprintf(stderr, "\nCaught signal: %d\n", signo);

    keep_running = 0;
}

static void help(void)
{
    static const char help_string[] =
        "\n Usage: audio_client [options]\n"
        "\n Possible options are:\n\n"
        "  -d <num>    Audio device index (see -l).\n"
        "  -r <num>    Audio sample rate (default is 48000).\n"
        "  -l          List audio devices.\n"
        "  -s <str>    Server IP (default is 127.0.0.1).\n"
        "  -p <num>    Network port number (default is 42001).\n"
        "  -h          This help message.\n\n";

    fprintf(stderr, "%s", help_string);
}

/* Parse command line options */
static void parse_options(int argc, char **argv, struct app_data *app)
{
    int             option;

    if (argc > 1)
    {
        while ((option = getopt(argc, argv, "d:r:ls:p:h")) != -1)
        {
            switch (option)
            {
            case 'd':
                app->device_index = atoi(optarg);
                break;

            case 'r':
                app->sample_rate = (uint32_t) atof(optarg);
                break;

            case 'l':
                audio_list_devices();
                exit(EXIT_SUCCESS);

            case 's':
                app->server_ip = strdup(optarg);
                break;

            case 'p':
                app->server_port = atoi(optarg);
                break;

            case 'h':
                help();
                exit(EXIT_SUCCESS);

            default:
                help();
                exit(EXIT_FAILURE);
            }

        }
    }
}


int main(int argc, char **argv)
{
    struct sockaddr_in serv_addr;
    struct pollfd   poll_fds[1];
    int             exit_code = EXIT_FAILURE;
    int             net_fd = -1;
    int             connected = 0;
    int             res;

    audio_t        *audio;
    OpusDecoder    *decoder;
    uint64_t        encoded_bytes = 0;
    uint64_t        decoder_errors = 0;
    int             error;
    uint64_t        iter = 0;

    struct app_data app = {
        .sample_rate = 48000,
        .device_index = -1,
        .server_port = DEFAULT_AUDIO_PORT,
    };

    parse_options(argc, argv, &app);
    if (app.server_ip == NULL)
        app.server_ip = strdup("127.0.0.1");

    fprintf(stderr, "Using server IP %s\n", app.server_ip);
    fprintf(stderr, "using server port %d\n", app.server_port);

    /* initialize audio subsystem */
    audio = audio_init(app.device_index, app.sample_rate, AUDIO_CONF_OUTPUT);
    if (audio == NULL)
        exit(EXIT_FAILURE);

    decoder = opus_decoder_create(app.sample_rate, 1, &error);
    if (error != OPUS_OK)
    {
        fprintf(stderr, "Error creating opus decoder: %d (%s)\n",
                error, opus_strerror(error));
        audio_close(audio);
        exit(EXIT_FAILURE);
    }

    /* setup signal handler */
    if (signal(SIGINT, signal_handler) == SIG_ERR)
        printf("Warning: Can't catch SIGINT\n");
    if (signal(SIGTERM, signal_handler) == SIG_ERR)
        printf("Warning: Can't catch SIGTERM\n");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(app.server_port);
    if (inet_pton(AF_INET, app.server_ip, &serv_addr.sin_addr) == -1)
    {
        fprintf(stderr, "Error calling inet_pton(): %d: %s\n", errno,
                strerror(errno));
        goto cleanup;
    }

    while (keep_running)
    {
        if (net_fd == -1)
        {
            net_fd = socket(AF_INET, SOCK_STREAM, 0);
            if (net_fd == -1)
            {
                fprintf(stderr, "Error creating socket: %d: %s\n", errno,
                        strerror(errno));
                goto cleanup;
            }
        }

        /* Try to connect to server */
        if (connect(net_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))
            == -1)
        {
            fprintf(stderr, "Connect error %d: %s\n", errno, strerror(errno));

            /* These errors may be temporary; try again */
            if (errno == ECONNREFUSED || errno == ENETUNREACH ||
                errno == ETIMEDOUT)
            {
                sleep(1);
                continue;
            }
            else
            {
                goto cleanup;
            }
        }

        poll_fds[0].fd = net_fd;
        poll_fds[0].events = POLLIN;
        connected = 1;
        fprintf(stderr, "Connected...\n");


        /* Ensure buffer has some data before we start playback */
        /* start audio playback */
        //audio_start(audio);

        while (keep_running && connected)
        {
            res = poll(poll_fds, 1, 500);

            if (res <= 0)
                continue;

            /* service network socket */
            if (poll_fds[0].revents & POLLIN)
            {

#define AUDIO_FRAMES 5760       // allows receiving up to 120 msec frames
#define AUDIO_BUFLEN 2 * AUDIO_FRAMES   // 120 msec: 48000 * 0.12
                uint8_t         buffer1[AUDIO_BUFLEN];
                uint8_t         buffer2[AUDIO_BUFLEN * 2];
                uint16_t        length;

                int             num;

                /* read 2 byte header */
                num = read(net_fd, buffer1, 2);
                if (num != 2)
                {
                    /* unrecovarable error; disconnect */
                    fprintf(stderr, "Error reading packet header: %d\n", num);
                    close(net_fd);
                    net_fd = -1;
                    connected = 0;
                    poll_fds[0].fd = -1;
                    audio_stop(audio);

                    num = opus_decode(decoder, NULL, 0, (opus_int16 *) buffer2,
                                      AUDIO_FRAMES, 0);

                    continue;
                }

                length = buffer1[0] + ((buffer1[1] & 0x1F) << 8);
                length -= 2;
                num = read(net_fd, buffer1, length);

                if (num == length)
                {
                    encoded_bytes += num;
                    num = opus_decode(decoder, buffer1, num,
                                      (opus_int16 *) buffer2, AUDIO_FRAMES, 0);
                    //fprintf(stderr, "REC: %d -> %d\n", length+2, num);
                    if (num > 0)
                    {
                        /** FIXME **/
                        audio_write_frames(audio, buffer2, num);
                        //Pa_WriteStream(audio->stream, buffer2, num);

                        /* start playback when there is 160 msec data */
                        iter++;
                        if (iter == 4)
                        {
                            audio_start(audio);
                        }

                    }
                    else
                    {
                        decoder_errors++;
                        fprintf(stderr, "Decoder error: %d (%s)\n", num,
                                opus_strerror(num));
                    }
                }
                else if (num == 0)
                {
                    fprintf(stderr, "Connection closed (FD=%d)\n", net_fd);
                    close(net_fd);
                    net_fd = -1;
                    connected = 0;
                    poll_fds[0].fd = -1;
                    audio_stop(audio);
                }
                else
                {
                    fprintf(stderr, "Error reading from net: %d / \n", num);
                }

            }

            //usleep(10000);
        }
    }

    fprintf(stderr, "Shutting down...\n");
    exit_code = EXIT_SUCCESS;

  cleanup:
    close(net_fd);
    if (app.server_ip != NULL)
        free(app.server_ip);

    audio_stop(audio);
    audio_close(audio);
    opus_decoder_destroy(decoder);

    fprintf(stderr, "  Encoded bytes in: %" PRIu64 "\n", encoded_bytes);
    fprintf(stderr, "  Decoder errors  : %" PRIu64 "\n", decoder_errors);

    exit(exit_code);
}
