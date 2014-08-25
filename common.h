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

/* Use 1 = debug, 0 = release */
#define DEBUG 1

/* timeout for select calls */
#define SELECT_TIMEOUT_SEC  1
#define SELECT_TIMEOUT_USEC 0

/* Amount of time in micorseconds we sleep in the main loop between cycles */
#define LOOP_DELAY_US 10000

/* default network ports */
#define DEFAULT_CTL_PORT   42000
#define DEFAULT_AUDIO_PORT 42001

inline void print_buffer(int from, int to, const char *buf, int len);
int set_serial_config(int fd, int speed, int parity, int blocking);
inline void transfer_data(int input_fd, int output_fd);

#endif
