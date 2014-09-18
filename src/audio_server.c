/*
 * Copyright (c) 2014, Alexandru Csete <oz9aec@gmail.com>
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
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include "common.h"

static int      port = DEFAULT_AUDIO_PORT;      /* Network port */
static int      keep_running = 1;       /* set to 0 to exit infinite loop */


void signal_handler(int signo)
{
    if (signo == SIGINT)
        fprintf(stderr, "\nCaught SIGINT\n");
    else if (signo == SIGTERM)
        fprintf(stderr, "\nCaught SIGTERM\n");
    else
        fprintf(stderr, "\nCaught signal: %d\n", signo);

    keep_running = 0;
}

static void help(void)
{
    static const char help_string[] =
        "\n Usage: audio_server [options]\n"
        "\n Possible options are:\n"
        "\n"
        "  -p    Network port number (default is 42001).\n"
        "  -h    This help message.\n\n";

    fprintf(stderr, "%s", help_string);
}

/* Parse command line options */
static void parse_options(int argc, char **argv)
{
    int             option;

    if (argc > 1)
    {
        while ((option = getopt(argc, argv, "p:h")) != -1)
        {
            switch (option)
            {
            case 'p':
                port = atoi(optarg);
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
    int             sock_fd, net_fd;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t       cli_addr_len;

    struct timeval  timeout;
    fd_set          active_fds, read_fds;
    int             res;
    int             connected;

    struct xfr_buf  net_in_buf;

    net_in_buf.wridx = 0;
    net_in_buf.valid_pkts = 0;
    net_in_buf.invalid_pkts = 0;


    /* setup signal handler */
    if (signal(SIGINT, signal_handler) == SIG_ERR)
        printf("Warning: Can't catch SIGINT\n");
    if (signal(SIGTERM, signal_handler) == SIG_ERR)
        printf("Warning: Can't catch SIGTERM\n");

    parse_options(argc, argv);
    fprintf(stderr, "Using network port %d\n", port);


    /* open and configure network interface */
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1)
    {
        fprintf(stderr, "Error creating socket: %d: %s\n", errno,
                strerror(errno));
        goto cleanup;
    }

    int             yes = 1;

    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1)
        fprintf(stderr, "Error setting SO_REUSEADDR: %d: %s\n", errno,
                strerror(errno));

    /* bind socket to host address */
    memset(&serv_addr, 0, sizeof(struct sockaddr_in));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);
    if (bind(sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
    {
        fprintf(stderr, "bind() error: %d: %s\n", errno, strerror(errno));
        goto cleanup;
    }

    if (listen(sock_fd, 1) == -1)
    {
        fprintf(stderr, "listen() error: %d: %s\n", errno, strerror(errno));
        goto cleanup;
    }

    memset(&cli_addr, 0, sizeof(struct sockaddr_in));
    cli_addr_len = sizeof(cli_addr);

    FD_ZERO(&active_fds);
    FD_SET(sock_fd, &active_fds);

    connected = 0;

    while (keep_running)
    {

        /* previous select may have altered timeout */
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        read_fds = active_fds;

        res = select(FD_SETSIZE, &read_fds, NULL, NULL, &timeout);
        if (res <= 0)
            continue;

        /* service network socket */
        if (connected && FD_ISSET(net_fd, &read_fds))
        {
            switch (read_data(net_fd, &net_in_buf))
            {
            case PKT_TYPE_EOF:
                fprintf(stderr, "Connection closed (FD=%d)\n", net_fd);
                FD_CLR(net_fd, &active_fds);
                close(net_fd);
                net_fd = -1;
                connected = 0;
                break;
            }

            net_in_buf.wridx = 0;
        }

        /* check if there are any new connections pending */
        if (FD_ISSET(sock_fd, &read_fds))
        {
            int             new;

            new = accept(sock_fd, (struct sockaddr *)&cli_addr, &cli_addr_len);
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
                net_fd = new;
                FD_SET(net_fd, &active_fds);
                connected = 1;
            }
            else
            {
                fprintf(stderr, "Connection refused\n");
                close(new);
            }
        }

        usleep(10000);
    }


    fprintf(stderr, "Shutting down...\n");
    exit_code = EXIT_SUCCESS;

  cleanup:
    close(net_fd);
    close(sock_fd);

    exit(exit_code);
}
