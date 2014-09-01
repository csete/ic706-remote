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
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <termios.h>
#include <unistd.h>

#include "common.h"


static char    *uart = NULL;    /* UART port */
static char    *server_ip = NULL;       /* Server IP */
static int      server_port = 42000;    /* Network port */
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
        "\n Usage: ic706_client [options]\n"
        "\n Possible options are:\n\n"
        "  -s    Server IP (default is 127.0.0.1).\n"
        "  -p    Network port number (default is 42000).\n"
        "  -u    Uart port (default is /dev/ttyO1).\n"
        "  -h    This help message.\n\n";

    fprintf(stderr, "%s", help_string);
}

/* Parse command line options */
static void parse_options(int argc, char **argv)
{
    int             option;

    if (argc > 1)
    {
        while ((option = getopt(argc, argv, "s:p:u:h")) != -1)
        {
            switch (option)
            {
            case 's':
                server_ip = strdup(optarg);
                break;

            case 'p':
                server_port = atoi(optarg);
                break;

            case 'u':
                uart = strdup(optarg);
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
    int             net_fd = -1;
    int             uart_fd;
    int             connected = 0;
    struct sockaddr_in serv_addr;
    struct xfr_buf  uart_buf, net_buf;
    fd_set          readfds;
    int             maxfd;

    struct timeval  timeout;
    int             res;


    /* initialize buffers */
    uart_buf.wridx = 0;
    uart_buf.valid_pkts = 0;
    uart_buf.invalid_pkts = 0;
    net_buf.wridx = 0;
    net_buf.valid_pkts = 0;
    net_buf.invalid_pkts = 0;

    /* setup signal handler */
    if (signal(SIGINT, signal_handler) == SIG_ERR)
        printf("Warning: Can't catch SIGINT\n");
    if (signal(SIGTERM, signal_handler) == SIG_ERR)
        printf("Warning: Can't catch SIGTERM\n");

    parse_options(argc, argv);
    if (uart == NULL)
        uart = strdup("/dev/ttyO1");
    if (server_ip == NULL)
        server_ip = strdup("127.0.0.1");

    fprintf(stderr, "Using UART %s\n", uart);
    fprintf(stderr, "Using server IP %s\n", server_ip);
    fprintf(stderr, "using server port %d\n", server_port);

    /* open and configure serial interface */
    uart_fd = open(uart, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (uart_fd == -1)
    {
        fprintf(stderr, "Error opening UART: %d: %s\n", errno,
                strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* 19200 bps, 8n1, blocking */
    if (set_serial_config(uart_fd, B19200, 0, 1) == -1)
    {
        fprintf(stderr, "Error configuring UART: %d: %s\n", errno,
                strerror(errno));
        goto cleanup;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) == -1)
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

        connected = 1;
        fprintf(stderr, "Connected...\n");

        maxfd = (net_fd > uart_fd ? net_fd : uart_fd) + 1;

        while (keep_running && connected)
        {
            FD_SET(net_fd, &readfds);
            FD_SET(uart_fd, &readfds);

            /* previous select may have altered timeout */
            timeout.tv_sec = SELECT_TIMEOUT_SEC;
            timeout.tv_usec = SELECT_TIMEOUT_USEC;
            res = select(maxfd, &readfds, NULL, NULL, &timeout);

            if (res <= 0)
                continue;

            /* service network socket */
            if (FD_ISSET(net_fd, &readfds))
            {
                if (transfer_data(net_fd, uart_fd, &net_buf) == PKT_TYPE_EOF)
                {
                    fprintf(stderr, "Connection closed (FD=%d)\n", net_fd);
                    FD_CLR(net_fd, &readfds);
                    close(net_fd);
                    net_fd = -1;
                    connected = 0;
                }
            }

            /* service UART port */
            if (FD_ISSET(uart_fd, &readfds))
                transfer_data(uart_fd, net_fd, &uart_buf);

            usleep(LOOP_DELAY_US);
        }
    }

    fprintf(stderr, "Shutting down...\n");
    exit_code = EXIT_SUCCESS;

  cleanup:
    close(net_fd);
    close(uart_fd);
    if (uart != NULL)
        free(uart);
    if (server_ip != NULL)
        free(server_ip);

    fprintf(stderr, "  Valid packets uart / net: %6ld / %6ld\n",
            uart_buf.valid_pkts, net_buf.valid_pkts);
    fprintf(stderr, "Invalid packets uart / net: %6ld / %6ld\n",
            uart_buf.invalid_pkts, net_buf.invalid_pkts);

    exit(exit_code);;
}
