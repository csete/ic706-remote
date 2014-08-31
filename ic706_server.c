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


static char *uart = NULL;       /* UART port */
static int   port = 42000;      /* Network port */
static int   keep_running = 1;  /* set to 0 to exit infinite loop */


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
        "\n Usage: ic706_server [options]\n"
        "\n Possible options are:\n"
        "\n"
        "  -p    Network port number (default is 42000).\n"
        "  -u    Uart port (default is /dev/ttyO1).\n"
        "  -h    This help message.\n"
        "\n";

    fprintf(stderr, "%s", help_string);
}

/* Parse command line options */
static void parse_options(int argc, char **argv)
{
    int option;

    if (argc > 1)
    {
        while ((option = getopt(argc, argv, "p:u:h")) != -1)
        {
            switch (option)
            {
            case 'p':
                port = atoi(optarg);
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
    int    exit_code = EXIT_FAILURE;
    int    sock_fd, net_fd, uart_fd;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t    cli_addr_len;

    struct timeval timeout;
    fd_set active_fds, read_fds;
    int    res = 0;
    int    connected = 0;


    struct xfr_buf uart_buf, net_buf;

    /* setup signal handler */
    if (signal(SIGINT, signal_handler) == SIG_ERR)
        printf("Warning: Can't catch SIGINT\n");
    if (signal(SIGTERM, signal_handler) == SIG_ERR)
        printf("Warning: Can't catch SIGTERM\n");
    
    parse_options(argc, argv);
    if (uart == NULL)
        uart = strdup("/dev/ttyO1");

    fprintf(stderr, "Using network port %d\n", port);
    fprintf(stderr, "Using UART port %s\n", uart);

    /* open and configure serial interface */
    uart_fd = open(uart, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (uart_fd == -1)
    {
        fprintf(stderr, "Error opening UART: %d: %s\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* 19200 bps, 8n1, blocking */
    if (set_serial_config(uart_fd, B19200, 0, 1) == -1)
    {
        fprintf(stderr, "Error configuring UART: %d: %s\n", errno,
                strerror(errno));
        goto cleanup;
    }

    /* open and configure network interface */
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1)
    {
        fprintf(stderr, "Error creating socket: %d: %s\n", errno,
                strerror(errno));
        goto cleanup;
    }

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

    /* initialize buffers */
    uart_buf.wridx = 0;
    uart_buf.valid_pkts = 0;
    uart_buf.invalid_pkts = 0;
    net_buf.wridx = 0;
    net_buf.valid_pkts = 0;
    net_buf.invalid_pkts = 0;


    FD_ZERO(&active_fds);
    FD_SET(uart_fd, &active_fds);
    FD_SET(sock_fd, &active_fds);

    while (keep_running)
    {
        /* previous select may have altered timeout */
        timeout.tv_sec  = SELECT_TIMEOUT_SEC;
        timeout.tv_usec = SELECT_TIMEOUT_USEC;
        read_fds = active_fds;

        res = select(FD_SETSIZE, &read_fds, NULL, NULL, &timeout);

        if (res > 0)
        {
            if (FD_ISSET(uart_fd, &read_fds))
                transfer_data(uart_fd, net_fd, &uart_buf);

            if (connected && FD_ISSET(net_fd, &read_fds))
            {
                if (transfer_data(net_fd, uart_fd, &net_buf) == PKT_TYPE_EOF)
                {
                    fprintf(stderr, "Connection closed (FD=%d)\n", net_fd);
                    FD_CLR(net_fd, &active_fds);
                    close(net_fd);
                    net_fd = -1;
                    connected = 0;
                }
            }

            if (FD_ISSET(sock_fd, &read_fds))
            {
                /* new connection */
                int new = accept(sock_fd, (struct sockaddr *) &cli_addr,
                                 &cli_addr_len);

                if (new == -1)
                {
                    fprintf(stderr, "accept() error: %d: %s\n",
                            errno, strerror(errno));
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
                    /* refuse connection */
                    close(new);
                    fprintf(stderr, "Connection refused\n");
                }
            }
        }

        usleep(LOOP_DELAY_US);
    }


    fprintf(stderr, "Shutting down...\n");
    exit_code = EXIT_SUCCESS;

cleanup:
    close(uart_fd);
    close(net_fd);
    close(sock_fd);
    if (uart != NULL)
        free(uart);

    fprintf(stderr, "  Valid packets uart / net: %6ld / %6ld\n",
            uart_buf.valid_pkts, net_buf.valid_pkts);
    fprintf(stderr, "Invalid packets uart / net: %6ld / %6ld\n",
            uart_buf.invalid_pkts, net_buf.invalid_pkts);

    exit(exit_code);
}
