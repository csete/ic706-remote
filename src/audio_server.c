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
#include <unistd.h>

#include "audio_util.h"
#include "common.h"


/* application state and config */
struct app_data {
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
        "  -d <num>    Audio device index (see -l).\n"
        "  -l          List audio devices.\n"
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
        while ((option = getopt(argc, argv, "d:hlp:")) != -1)
        {
            switch (option)
            {
            case 'd':
                app->device_index = atoi(optarg);
                break;

            case 'l':
                audio_list_devices();
                exit(EXIT_SUCCESS);

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


int main(int argc, char **argv)
{
    int             exit_code = EXIT_FAILURE;
    int             sock_fd;
    struct sockaddr_in cli_addr;
    socklen_t       cli_addr_len;

    struct pollfd   poll_fds[2];
    int             connected;

    audio_t        *audio;

    struct app_data app = {
        .device_index = -1,
        .network_port = DEFAULT_AUDIO_PORT,
    };

    struct xfr_buf  net_in_buf;

    net_in_buf.wridx = 0;
    net_in_buf.write_errors = 0;
    net_in_buf.valid_pkts = 0;
    net_in_buf.invalid_pkts = 0;

    parse_options(argc, argv, &app);
    fprintf(stderr, "Using network port %d\n", app.network_port);

    /* initialize audio subsystem */
    audio = audio_init(app.device_index, AUDIO_CONF_INPUT);
    if (audio == NULL)
        exit(EXIT_FAILURE);

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
            uint8_t         buffer[AUDIO_BUFLEN];
            uint32_t        frames_read;

            /** FIXME: We should read all frames modulo encoder frame */
            if (audio_frames_available(audio) < AUDIO_FRAMES)
                continue;

            frames_read = audio_read_frames(audio, buffer, AUDIO_FRAMES);

            if (frames_read != AUDIO_FRAMES)
            {
                fprintf(stderr,
                        "Error reading audio (got %d instead of %d frames)\n",
                        frames_read, AUDIO_FRAMES);
            }
            else
            {
                if (write(poll_fds[1].fd, buffer, AUDIO_FRAMES * 2) < 0)
                    fprintf(stderr, "Error writing audio to network socket\n");
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

    //fprintf(stderr, "  Audio bytes: %" PRIu64 "\n", abuf.bytes_read);
    //fprintf(stderr, "  Average read: %" PRIu64 "\n", abuf.avg_read);

    exit(exit_code);
}
