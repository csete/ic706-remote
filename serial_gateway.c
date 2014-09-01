/*
 * Copyright (c) 2014, Alexandru Csete <oz9aec@gmail.com>
 * All rights reserved.
 *
 * This software is licensed under the terms and conditions of the
 * Simplified BSD License. See license.txt for details.
 *
 */
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>

#include "common.h"


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



int main(int argc, char **argv)
{
    char           *panel_port = "/dev/ttyUSB0";
    int             panel_fd;
    fd_set          readfs;     /* file descriptor set */
    int             maxfd;      /* maximum file desciptor used */
    struct timeval  timeout;
    int             res;

    struct xfr_buf  radio_buf, panel_buf;


    /* setup signal handler */
    if (signal(SIGINT, signal_handler) == SIG_ERR)
        printf("Warning: Can't catch SIGINT\n");
    if (signal(SIGTERM, signal_handler) == SIG_ERR)
        printf("Warning: Can't catch SIGTERM\n");

    /* control panel */
    panel_fd = open(panel_port, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (panel_fd < 0)
    {
        fprintf(stderr, "error %d opening %s: %s", errno, panel_port,
                strerror(errno));
        return 1;
    }
    /* 19200 bps, 8n1, blocking */
    set_serial_config(panel_fd, B19200, 0, 1);

    /* radio end */
    char           *radio_port = "/dev/ttyUSB1";
    int             radio_fd =
        open(radio_port, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (radio_fd < 0)
    {
        fprintf(stderr, "error %d opening %s: %s", errno, radio_port,
                strerror(errno));

        goto closefds;
    }

    /* 19200 bps, 8n1, blocking */
    set_serial_config(radio_fd, B19200, 0, 1);

    /* maximum bit entry (fd) to test */
    maxfd = (radio_fd > panel_fd ? radio_fd : panel_fd) + 1;

    radio_buf.wridx = 0;
    radio_buf.valid_pkts = 0;
    radio_buf.invalid_pkts = 0;
    panel_buf.wridx = 0;
    panel_buf.valid_pkts = 0;
    panel_buf.invalid_pkts = 0;

    while (keep_running)
    {
        FD_SET(panel_fd, &readfs);      /* set testing for source 1 */
        FD_SET(radio_fd, &readfs);      /* set testing for source 2 */

        timeout.tv_sec = SELECT_TIMEOUT_SEC;
        timeout.tv_usec = SELECT_TIMEOUT_USEC;

        /* block until input becomes available */
        res = select(maxfd, &readfs, NULL, NULL, &timeout);

        if (res > 0)
        {
            if (FD_ISSET(panel_fd, &readfs))
                transfer_data(panel_fd, radio_fd, &panel_buf);

            if (FD_ISSET(radio_fd, &readfs))
                transfer_data(radio_fd, panel_fd, &radio_buf);
        }

        usleep(LOOP_DELAY_US);
    }


  closefds:
    close(panel_fd);
    close(radio_fd);

    fprintf(stderr, "  Valid packets radio / panel: %6ld / %6ld\n",
            radio_buf.valid_pkts, panel_buf.valid_pkts);
    fprintf(stderr, "Invalid packets radio / panel: %6ld / %6ld\n",
            radio_buf.invalid_pkts, panel_buf.invalid_pkts);

    return 0;
}
