/*
 * Copyright (c) 2014, Alexandru Csete <oz9aec@gmail.com>
 * All rights reserved.
 *
 * This software is licensed under the terms and conditions of the
 * Simplified BSD License. See license.txt for details.
 *
 */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>

#include "common.h"

/* Print an array of chars as HEX numbers */
inline void print_buffer(int from, int to, const uint8_t * buf,
                         unsigned int len)
{
    int             i;

    fprintf(stderr, "%d -> %d:", from, to);

    for (i = 0; i < len; i++)
        fprintf(stderr, " %02X", buf[i]);

    fprintf(stderr, "\n");
}

/* Configure serial interface to raw mode with specified attributes */
int set_serial_config(int fd, int speed, int parity, int blocking)
{
    struct termios  tty;

    memset(&tty, 0, sizeof(tty));
    if (tcgetattr(fd, &tty) != 0)
    {
        fprintf(stderr, "error %d from tcgetattr", errno);
        return -1;
    }

    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8; // 8-bit chars
    // disable IGNBRK for mismatched speed tests; otherwise receive break
    // as \000 chars
    tty.c_iflag &= ~IGNBRK;     // disable break processing
    tty.c_lflag = 0;            // no signaling chars, no echo,
    // no canonical processing

    /* no remapping, no delays */
    tty.c_oflag = 0;

    /* 0.5 sec read timeout */
    tty.c_cc[VMIN] = blocking ? 1 : 0;
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

    if (tcsetattr(fd, TCSANOW, &tty) != 0)
    {
        fprintf(stderr, "error %d from tcsetattr", errno);
        return -1;
    }

    return 0;
}


int read_data(int fd, struct xfr_buf *buffer)
{
    uint8_t        *buf = buffer->data;
    int             type = PKT_TYPE_INCOMPLETE;
    size_t          num;

    /* read data */
    num = read(fd, &buf[buffer->wridx], RDBUF_SIZE - buffer->wridx);

    if (num > 0)
    {
        buffer->wridx += num;

        /* There is at least one character in the buffer.
         *
         * If buf[0] = 0xFE then this is a regular packet. Check if
         * buf[end] = 0xFD, if yes, the packet is complete and return
         * the packet type.
         *
         * If buf[0] = 0x00 and wridx = 1 then this is an EOS packet.
         * If buf[0] = 0x00 and wridx > 1 then this is an invalid
         * packet (does not start with 0xFE).
         */
        if (buf[0] == 0xFE)
        {
            if (buf[buffer->wridx - 1] == 0xFD)
                type = buf[1];
            else
                type = PKT_TYPE_INCOMPLETE;
        }
        else if ((buf[0] == 0x00) && (buffer->wridx == 1))
        {
            type = PKT_TYPE_EOS;
        }
        else
        {
            type = PKT_TYPE_INVALID;
        }
    }
    else if (num == 0)
    {
        type = PKT_TYPE_EOF;
        fprintf(stderr, "Received EOF from FD %d\n", fd);
    }
    else
    {
        type = PKT_TYPE_INVALID;
        fprintf(stderr, "Error reading from FD %d: %d: %s\n", fd, errno,
                strerror(errno));
    }

    return type;
}


int transfer_data(int ifd, int ofd, struct xfr_buf *buffer)
{
    int             pkt_type;

    pkt_type = read_data(ifd, buffer);
    switch (pkt_type)
    {
    case PKT_TYPE_INCOMPLETE:
        break;

    case PKT_TYPE_INVALID:
        buffer->invalid_pkts++;
        buffer->wridx = 0;
        break;

    default:
        /* we also "send" on EOF packet because buffer may not be empty */
#if DEBUG
        print_buffer(ifd, ofd, buffer->data, buffer->wridx);
#endif
        write(ofd, buffer->data, buffer->wridx);
        buffer->wridx = 0;
        buffer->valid_pkts++;
    }

    return pkt_type;
}


uint64_t time_ms(void)
{
    struct timeval  tval;

    gettimeofday(&tval, NULL);

    return 1e3 * tval.tv_sec + 1e-3 * tval.tv_usec;
}

uint64_t time_us(void)
{
    struct timeval  tval;

    gettimeofday(&tval, NULL);

    return 1e6 * tval.tv_sec + tval.tv_usec;
}
