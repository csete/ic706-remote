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
#include <signal.h>
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
        while ((option = getopt(argc, argv, "d:ls:p:h")) != -1)
        {
            switch (option)
            {
            case 'd':
                app->device_index = atoi(optarg);
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

    struct app_data app = {
        .device_index = -1,
        .server_port = DEFAULT_AUDIO_PORT,
    };

    struct xfr_buf  net_in_buf = {
        .wridx = 0,
        .write_errors = 0,
        .valid_pkts = 0,
        .invalid_pkts = 0,
    };

    parse_options(argc, argv, &app);
    if (app.server_ip == NULL)
        app.server_ip = strdup("127.0.0.1");

    fprintf(stderr, "Using server IP %s\n", app.server_ip);
    fprintf(stderr, "using server port %d\n", app.server_port);

    /* initialize audio subsystem */
    audio = audio_init(app.device_index, AUDIO_CONF_OUTPUT);
    if (audio == NULL)
        exit(EXIT_FAILURE);

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
        audio_start(audio);

        while (keep_running && connected)
        {
            res = poll(poll_fds, 1, 500);

            if (res <= 0)
                continue;

            /* service network socket */
            if (poll_fds[0].revents & POLLIN)
            {
                int             num;

                num = read(net_fd, net_in_buf.data, RDBUF_SIZE);
                if (num > 0)
                {
                    net_in_buf.valid_pkts += num;
                    //audio_write_frames(audio, net_in_buf.data, num / 2); /** FIXME */
                    Pa_WriteStream(audio->stream, net_in_buf.data, num / 2);
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
                    fprintf(stderr, "Error reading from network socket\n");
                }

            }

            usleep(10000);
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

    fprintf(stderr, "  Valid packets net: %" PRIu64 "\n",
            net_in_buf.valid_pkts);

    exit(exit_code);
}
