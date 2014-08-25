/*
 * Copyright (c) 2014, Alexandru Csete <oz9aec@gmail.com>
 * All rights reserved.
 *
 * This software is licensed under the terms and conditions of the
 * Simplified BSD License. See license.txt for details.
 *
 */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "common.h"

/* Print an array of chars as HEX numbers */
inline void print_buffer(int from, int to, const char *buf, int len)
{
    int i;

    fprintf(stderr, "%d -> %d:", from, to);

    for (i = 0; i < len; i++)
    {
        fprintf(stderr, " %02X", (unsigned char)buf[i]);
    }
    fprintf(stderr, "\n");
}

/* Configure serial interface to raw mode with specified attributes */
int set_serial_config(int fd, int speed, int parity, int blocking)
{
    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    if (tcgetattr (fd, &tty) != 0)
    {
        fprintf(stderr, "error %d from tcgetattr", errno);
        return -1;
    }

    cfsetospeed (&tty, speed);
    cfsetispeed (&tty, speed);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
    // disable IGNBRK for mismatched speed tests; otherwise receive break
    // as \000 chars
    tty.c_iflag &= ~IGNBRK;         // disable break processing
    tty.c_lflag = 0;                // no signaling chars, no echo,
                                    // no canonical processing

    /* no remapping, no delays */
    tty.c_oflag = 0;

    /* 0.5 sec read timeout */
    tty.c_cc[VMIN]  = blocking ? 1 : 0;
    tty.c_cc[VTIME] = 5;

    /* shut off xon/xoff ctrl */
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);

    /* ignore modem controls and enable reading */
    tty.c_cflag |= (CLOCAL | CREAD);

    /* parity */
    tty.c_cflag &= ~(PARENB | PARODD);
    tty.c_cflag |= parity;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;

    if (tcsetattr (fd, TCSANOW, &tty) != 0)
    {
        fprintf(stderr, "error %d from tcsetattr", errno);
        return -1;
    }

    return 0;
}

/* read data from input_fd and send it to output_fd */
inline void transfer_data(int input_fd, int output_fd)
{
    char data[512];

    size_t num = read(input_fd, data, 512);

    if (num > 0)
    {
        write(output_fd, data, num);

#if DEBUG
        print_buffer(input_fd, output_fd, data, num);
#endif
    }
    
}
