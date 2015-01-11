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
#include <unistd.h>

#include "audio_util.h"
#include "common.h"


/* application state and config */
struct app_data {
    int32_t         opus_bitrate;
    int32_t         opus_complexity;
    uint32_t        sample_rate;        /* audio sample rate */
    int             device_index;       /* audio device index */
    int             network_port;       /* network port number */
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
        "\n Usage: audio_server [options]\n"
        "\n Possible options are:\n"
        "\n"
        "  -d <num>  Audio device index (see -l).\n"
        "  -r <num>  Audio sample rate (default is 48000).\n"
        "  -l        List audio devices.\n"
        "  -b <num>  Opus encoder output rate in bits per sec (default is 16 kbps).\n"
        "  -c <num>  Opus encoder complexity 1-10 (default is 5).\n"
        "  -p <num>  Network port number (default is 42001).\n"
        "  -h        This help message.\n\n";

    fprintf(stderr, "%s", help_string);
}

/* Parse command line options */
static void parse_options(int argc, char **argv, struct app_data *app)
{
    int             option;

    if (argc > 1)
    {
        while ((option = getopt(argc, argv, "d:r:lb:c:p:h")) != -1)
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

            case 'b':
                app->opus_bitrate = (int32_t) atof(optarg);
                break;

            case 'c':
                app->opus_complexity = atoi(optarg);
                break;

            case 'p':
                app->network_port = atoi(optarg);
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

static void setup_encoder(OpusEncoder * encoder, struct app_data *app)
{
    opus_int32      x;

    fprintf(stderr, "Configuring opus encoder:\n");

    opus_encoder_ctl(encoder, OPUS_SET_MAX_BANDWIDTH(OPUS_BANDWIDTH_WIDEBAND));
    opus_encoder_ctl(encoder, OPUS_SET_BITRATE(app->opus_bitrate));
    opus_encoder_ctl(encoder, OPUS_SET_COMPLEXITY(app->opus_complexity));

    opus_encoder_ctl(encoder, OPUS_GET_COMPLEXITY(&x));
    fprintf(stderr, "  Complexity: %d\n", x);
    opus_encoder_ctl(encoder, OPUS_GET_BITRATE(&x));
    fprintf(stderr, "  Bitrate   : %d\n", x);
}

int main(int argc, char **argv)
{
    int             exit_code = EXIT_FAILURE;
    int             sock_fd;
    struct sockaddr_in cli_addr;
    socklen_t       cli_addr_len;

    struct pollfd   poll_fds[2];
    int             connected;

    audio_t        *audio;
    OpusEncoder    *encoder;
    uint64_t        encoded_bytes = 0;
    uint64_t        encoder_errors = 0;
    int             error;


    struct app_data app = {
        .opus_bitrate = 16000,
        .opus_complexity = 5,
        .sample_rate = 48000,
        .device_index = -1,
        .network_port = DEFAULT_AUDIO_PORT,
    };

    struct xfr_buf  net_in_buf = {
        .wridx = 0,
        .write_errors = 0,
        .valid_pkts = 0,
        .invalid_pkts = 0,
    };

    parse_options(argc, argv, &app);
    fprintf(stderr, "Using network port %d\n", app.network_port);

    /* initialize audio subsystem */
    audio = audio_init(app.device_index, app.sample_rate, AUDIO_CONF_INPUT);
    if (audio == NULL)
        exit(EXIT_FAILURE);

    /* audio encoder */
    encoder = opus_encoder_create(app.sample_rate, 1,
                                  OPUS_APPLICATION_AUDIO, &error);
    if (error != OPUS_OK)
    {
        fprintf(stderr, "Error creating opus encoder: %d (%s)\n",
                error, opus_strerror(error));
        audio_close(audio);
        exit(EXIT_FAILURE);
    }
    setup_encoder(encoder, &app);

    /* setup signal handler */
    if (signal(SIGINT, signal_handler) == SIG_ERR)
        printf("Warning: Can't catch SIGINT\n");
    if (signal(SIGTERM, signal_handler) == SIG_ERR)
        printf("Warning: Can't catch SIGTERM\n");

    /* network socket (listening for connections) */
    sock_fd = create_server_socket(app.network_port);
    poll_fds[0].fd = sock_fd;
    poll_fds[0].events = POLLIN;

    /* network socket to client (when connected)
     * FIXME: we could use it to check when writing is wont block
     */
    poll_fds[1].fd = -1;
    memset(&cli_addr, 0, sizeof(struct sockaddr_in));
    cli_addr_len = sizeof(cli_addr);
    connected = 0;

    while (keep_running)
    {

        if (poll(poll_fds, 2, 10) < 0)
            continue;

        /* service network socket */
        if (connected && (poll_fds[1].revents & POLLIN))
        {
            switch (read_data(poll_fds[1].fd, &net_in_buf))
            {
            case PKT_TYPE_EOF:
                fprintf(stderr, "Connection closed (FD=%d)\n", poll_fds[1].fd);
                close(poll_fds[1].fd);
                poll_fds[1].fd = -1;
                poll_fds[1].events = 0;
                connected = 0;
                audio_stop(audio);
                break;
            }

            net_in_buf.wridx = 0;
        }

        /* check if there are any new connections pending */
        if (poll_fds[0].revents & POLLIN)
        {
            int             new;        /* FIXME: we can do it without 'new' */

            new = accept(poll_fds[0].fd, (struct sockaddr *)&cli_addr,
                         &cli_addr_len);
            if (new == -1)
            {
                fprintf(stderr, "accept() error: %d: %s\n", errno,
                        strerror(errno));
                goto cleanup;
            }

            fprintf(stderr, "New connection from %s\n",
                    inet_ntoa(cli_addr.sin_addr));

            if (!connected)
            {
                fprintf(stderr, "Connection accepted (FD=%d)\n", new);
                poll_fds[1].fd = new;
                poll_fds[1].events = POLLIN;

                connected = 1;
                audio_start(audio);
            }
            else
            {
                fprintf(stderr, "Connection refused\n");
                close(new);
            }
        }

        /* process available audio data */
        if (connected)
        {
#define AUDIO_FRAMES 1920       // 40 msek: 48000 * 0.04
#define AUDIO_BUFLEN 3840
            uint8_t         buffer1[AUDIO_BUFLEN];
            uint8_t         buffer2[AUDIO_BUFLEN + 2];
            uint16_t        length;

            if (audio_frames_available(audio) < AUDIO_FRAMES)
                continue;

            length = audio_read_frames(audio, buffer1, AUDIO_FRAMES);

            if (length != AUDIO_FRAMES)
            {
                fprintf(stderr,
                        "Error reading audio (got %d instead of %d frames)\n",
                        length, AUDIO_FRAMES);
            }
            else
            {
                /* encode audio frame (items 0, 1 are reserved for header) */
                length = opus_encode(encoder, (opus_int16 *) buffer1,
                                     AUDIO_FRAMES, &buffer2[2], AUDIO_BUFLEN);
                if (length > 0)
                {
                    encoded_bytes += length;

                    /* Add header according to RemoteSDR ICD:
                     *   byte 1: LSB of buffer length incl header
                     *   byte 2: 0x80 & 5 bit MSB of buffer length incl. header
                     */
                    length += 2;
                    buffer2[0] = (uint8_t) (length & 0xFF);
                    buffer2[1] = (uint8_t) (0x80 | ((length >> 8) & 0x1F));
                    if (write(poll_fds[1].fd, buffer2, length) < 0)
                        fprintf(stderr,
                                "Error writing audio to network socket\n");
                    //else
                    //    fprintf(stderr, "SENT: %d\n", length);
                }
                else
                {
                    encoder_errors++;
                    fprintf(stderr, "Encoder error: %d (%s)\n",
                            length, opus_strerror(length));
                }
            }

        }

    }

    fprintf(stderr, "Shutting down...\n");
    exit_code = EXIT_SUCCESS;

  cleanup:
    close(poll_fds[0].fd);
    close(poll_fds[1].fd);

    audio_stop(audio);
    audio_close(audio);

    opus_encoder_destroy(encoder);

    //fprintf(stderr, "  Audio bytes: %" PRIu64 "\n", abuf.bytes_read);
    //fprintf(stderr, "  Average read: %" PRIu64 "\n", abuf.avg_read);

    fprintf(stderr, "  Encoded bytes : %" PRIu64 "\n", encoded_bytes);
    fprintf(stderr, "  Encoder errors: %" PRIu64 "\n", encoder_errors);

    exit(exit_code);
}
