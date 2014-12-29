/*
 * Copyright (c) 2014, Alexandru Csete
 * All rights reserved.
 *
 * This software is licensed under the terms and conditions of the
 * Simplified BSD License. See license.txt for details.
 *
 */
#include <errno.h>
#include <fcntl.h>              /* O_WRONLY */
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
    uint8_t         init1_resp[] = { 0xFE, 0xF0, 0xFD };
    uint8_t         init2_resp[] = { 0xFE, 0xF1, 0xFD };
    int             pkt_type;

    pkt_type = read_data(ifd, buffer);
    switch (pkt_type)
    {
    case PKT_TYPE_KEEPALIVE:
        /* emulated on server side; do not forward */
        buffer->wridx = 0;
        buffer->valid_pkts++;

    case PKT_TYPE_INIT1:
        /* Sent by the first unit that is powered on.
           Expects PKT_TYPE_INIT1 + PKT_TYPE_INIT2 in response. */
        write(ifd, init1_resp, 3);
        write(ifd, init2_resp, 3);
        buffer->wridx = 0;
        buffer->valid_pkts++;
        break;

    case PKT_TYPE_INIT2:
        /* Sent by the panel when powered on and the radio is already on.
           Expects PKT_TYPE_INIT2 in response. */
        write(ifd, init2_resp, 3);
        buffer->wridx = 0;
        buffer->valid_pkts++;
        break;

    case PKT_TYPE_PWK:
        /* Power on/off message sent by panel; leave handling to server */
#if DEBUG
        print_buffer(ifd, ofd, buffer->data, buffer->wridx);
#endif
        buffer->wridx = 0;
        buffer->valid_pkts++;
        break;

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

void send_keepalive(int fd)
{
    char            msg[] = { 0xFE, 0x0B, 0x00, 0xFD };
    write(fd, msg, 4);
}

void send_pwr_message(int fd, int poweron)
{
    char            msg[] = { 0xFE, 0xA0, 0x00, 0xFD };

    if (poweron)
        msg[2] = 0x01;

    write(fd, msg, 4);
}

int pwk_init(void)
{
    int             fd;

    /*  $ echo 7 > /sys/class/gpio/export */
    fd = open("/sys/class/gpio/export", O_WRONLY);
    if (fd < 0)
        return -1;

    write(fd, "7", 1);
    close(fd);

    /*  $ echo "in" > /sys/class/gpio/gpio7/direction */
    fd = open("/sys/class/gpio/gpio7/direction", O_WRONLY);
    if (fd < 0)
        return -1;

    write(fd, "in", 2);
    close(fd);

    /*  $ echo 1 > /sys/class/gpio/gpio7/active_low */
    fd = open("/sys/class/gpio/gpio7/active_low", O_WRONLY);
    if (fd < 0)
        return -1;

    write(fd, "1", 1);
    close(fd);

    /*  $ echo "falling" > /sys/class/gpio/gpio7/edge  */
    fd = open("/sys/class/gpio/gpio7/edge", O_WRONLY);
    if (fd < 0)
        return -1;

    write(fd, "falling", 7);
    close(fd);

    fd = open("/sys/class/gpio/gpio7/value", O_RDONLY);

    return fd;
}


#define SYSFS_GPIO_DIR "/sys/class/gpio/"
#define MAX_GPIO_BUF   100

int gpio_init_out(unsigned int gpio)
{
    int             fd;
    int             len;
    char            buf[MAX_GPIO_BUF];

    /* export GPIO */
    fd = open(SYSFS_GPIO_DIR "export", O_WRONLY);
    if (fd < 0)
        return -1;

    len = snprintf(buf, sizeof(buf), "%d", gpio);
    write(fd, buf, len);
    close(fd);

    /* set direction to "out" */
    snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "gpio%d/direction", gpio);
    fd = open(buf, O_WRONLY);
    if (fd < 0)
        return -1;

    write(fd, "out", 3);
    close(fd);

    /* intialize with a 0 */
    if (gpio_set_value(20, 0) < 0)
        return -1;

    return 0;
}

int gpio_set_value(unsigned int gpio, unsigned int value)
{
    int             fd;
    char            buf[MAX_GPIO_BUF];

    snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "gpio%d/value", gpio);
    fd = open(buf, O_WRONLY);
    if (fd < 0)
        return -1;

    if (value == 1)
        write(fd, "1", 1);
    else
        write(fd, "0", 1);

    close(fd);

    return 0;
}
