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

#define socket_error() \
    do { strerror(errno); exit(EXIT_FAILURE); } while (0)


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
    int    sockfd, net_fd, uart_fd;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t    cli_addr_len;


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
        socket_error();

    /* 19200 bps, 8n1, blocking */
    set_serial_config(uart_fd, B19200, 0, 1);

    /* open and configure network interface */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
        socket_error();

    /* bind socket to host address */
    memset(&serv_addr, 0, sizeof(struct sockaddr_in));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
        socket_error();
 
    if (listen(sockfd, 5) == -1)
        socket_error();


    /* this blocks until a connection is opened */
    net_fd = accept(sockfd, (struct sockaddr *) &cli_addr, &cli_addr_len);
    if (net_fd == -1)
        socket_error();

    fprintf(stderr, "New connection from: %s\n", inet_ntoa(cli_addr.sin_addr));

    {
        fd_set readfds;
        int    maxfd;

        struct timeval timeout;
        int res;

        maxfd = (net_fd > uart_fd ? net_fd : uart_fd) + 1;

        while (keep_running)
        {
            FD_SET(net_fd, &readfds);
            FD_SET(uart_fd, &readfds);

            /* previous select may have altered timeout */
            timeout.tv_sec  = SELECT_TIMEOUT_SEC;
            timeout.tv_usec = SELECT_TIMEOUT_USEC;
            res = select(maxfd, &readfds, NULL, NULL, &timeout);

            if (res > 0)
            {
                if (FD_ISSET(net_fd, &readfds))
                    transfer_data(net_fd, uart_fd);

                if (FD_ISSET(uart_fd, &readfds))
                    transfer_data(uart_fd, net_fd);
            }

            usleep(LOOP_DELAY_US);
        }
    }

    free(uart);
    close(net_fd);
    close(uart_fd);

    return 0;
}
