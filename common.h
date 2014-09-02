/*
 * Copyright (c) 2014, Alexandru Csete <oz9aec@gmail.com>
 * All rights reserved.
 *
 * This software is licensed under the terms and conditions of the
 * Simplified BSD License. See license.txt for details.
 *
 */
#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdint.h>

/* Use 1 = debug, 0 = release */
#define DEBUG 1

/* Read buffer size */
#define RDBUF_SIZE 512

/* Amount of time in micorseconds we sleep in the main loop between cycles */
#define LOOP_DELAY_US 1000

/* default network ports */
#define DEFAULT_CTL_PORT   42000
#define DEFAULT_AUDIO_PORT 42001


/* The following lines define different types of packets sent between
 * radio and panel.
 *
 * The protocol used between the IC-706 radio and the front panel is
 * similar to the ICOM CIV protocol. IT starts with a 1 byte preamble
 * 0xFE and ends with 0xFD.
 *
 * The packet type is determined by the second byte in the packet right
 * after the preamble 0xFE. For example, PTT on and off have the
 * following sequences:
 *
 *     0xFE 0x00 0x01 0xFD    // PTT ON (pressed)
 *     0xFE 0x00 0x00 0xFD    // PTT OFF (released)
 *
 * See http://ok1zia.nagano.cz/wiki/Front_panel_IC-706 for more details
 * about the protocol. However, that page is missing PKT_TYPE_INIT1,
 * PKT_TYPE_INIT1 and PKT_TYPE_EOS.
 */
#define PKT_TYPE_PTT        0x00
#define PKT_TYPE_BUTTONS1   0x01
#define PKT_TYPE_BUTTONS2   0x02
#define PKT_TYPE_TUNE       0x03
#define PKT_TYPE_VOLUME     0x05
#define PKT_TYPE_RFSQL      0x06
#define PKT_TYPE_MEMCH      0x07
#define PKT_TYPE_SHIFT      0x08
#define PKT_TYPE_KEEPALIVE  0x0B
#define PKT_TYPE_LCD        0x60

/* 0xFE 0xF0 0xFD sent radio->panel 12 times during powerup */
#define PKT_TYPE_INIT1      0xF0

/* 0xFE 0xF1 0xFD sent both ways after INIT1 */
#define PKT_TYPE_INIT2      0xF1
#define PKT_TYPE_INCOMPLETE 0xFA

/* End of session: A single 0x00 sent both ways */
#define PKT_TYPE_EOS        0xFB
#define PKT_TYPE_EOF        0xFC        /* we use this indicate 0-bytes read */
#define PKT_TYPE_UNKNOWN    0xFE
#define PKT_TYPE_INVALID    0xFF

/* convenience struct for data transfers */
struct xfr_buf {
    uint8_t         data[RDBUF_SIZE];
    int             wridx;      /* next available write slot. */
    uint64_t        valid_pkts; /* number of valid packets */
    uint64_t        invalid_pkts;       /* number of invalid packets */
};

/** Read data from file descriptor.
 *  @param  fd      The file descriptor.
 *  @param  buffer  Pointer to the serial_buffer structure to use.
 *  @returns The packet type if the packet is complete.
 *
 * This function will read all available data from the UART and put the
 * data into the buffer starting at index buffer->wridx. When the read
 * is finished buffer->wridx will again point to the first available
 * slot in buffer, which mneans that it is also equal to the number of
 * bytes in the buffer.
 *
 * If the last byte read was 0xFD the packet is considered complete
 * and the function will return the packet type. The caller can use this
 * information to decide whether the packet should be forwarded to the
 * network interface.
 *
 * If the last byte is not 0xFD then the read is only partial and the
 * function returns PKT_TYPE_INCOMPLETE to indicate this to the caller.
 *
 * @bug We assume that 0xFD can only occur as the last byte during a
 *      read() op, which is not always the case.
 */
int             read_data(int fd, struct xfr_buf *buffer);

/**
 * Transfer data from one interface to the other.
 * @param ifd Input file descriptor.
 * @param ofd Output file descriptor.
 * @param buffer Pointer to the serial buffer structure use to collect
 *               packets from the serial port.
 * @return The packet type that was read.
 *
 * \todo Some packet type are transfered, others are not
 */
int             transfer_data(int ifd, int ofd, struct xfr_buf *buffer);

inline void     print_buffer(int from, int to, const uint8_t * buf,
                             unsigned int len);
int             set_serial_config(int fd, int speed, int parity, int blocking);

/** Get current time in milliseconds. */
uint64_t        time_ms(void);

/** Get current time in microseconds. */
uint64_t        time_us(void);

void            send_keepalive(int fd);

#endif
