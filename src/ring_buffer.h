/*
 * Simple ring buffer for nanosdr.
 * 
 * Copyright (c) 2014, Alexandru Csete
 * All rights reserved.
 *
 * This software is licensed under the terms and conditions of the
 * Simplified BSD License. See license.txt for details.
 */
#ifndef __RING_BUFFER_H__
#define __RING_BUFFER_H__

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/**
 * @file 
 * Simple ring buffer implementation.
 *
 * @author    Alexandru Csete
 * @date      2014/12/29
 * @copyright Simplified BSD License
 *
 * This file implements a simple ring buffer that can hold data of unsigned
 * char type. The present implementation does not contain any boundary checks
 * and the callers must ensure that (1) the number of elemets written is less
 * than or equal to the buffer size and (2) the umber of elements read from
 * the buffer is less than or equal to the number of elements currently in the
 * buffer.
 *
 * Furthermore, the buffer does not check how much empty space there is during
 * write operations, i.e. old data will be overwritten if writing happens
 * faster than reading.
 */

/**
 * The ring buffer structure.
 *
 * @buffer  The array of elements stored in the buffer.
 * @size    The sizeof the buffer.
 * @start   Index of the oldest element.
 * @count   Number of elements currently in the buffer.
 */
typedef struct {
    unsigned char  *buffer;
    uint_fast32_t   size;
    uint_fast32_t   start;
    uint_fast32_t   count;
} ring_buffer_t;


/**
 * Initialize the ring buffer.
 *
 * @param rb Pointer to a newly allocated ring_buffer_t structure.
 * @param size The size of the buffer.
 *
 * This function will allocate the memory for the internal buffer and
 * initialize the bookkeeping parameters.
 */
static inline void ring_buffer_init(ring_buffer_t * rb, uint_fast32_t size)
{
    rb->size = size;
    rb->start = 0;
    rb->count = 0;
    rb->buffer = (unsigned char *)malloc(rb->size);
}

static inline void ring_buffer_free(ring_buffer_t * rb)
{
    free(rb->buffer);
}

/**
 * Resize the ring buffer.
 *
 * @param  rb  Pointer to the ring buffer
 * @param  newsize The new size desired for the buffer.
 *
 * @warning The internal buffer will be reallocated and all data lost during
 *          this process.
 */
static inline void ring_buffer_resize(ring_buffer_t * rb,
                                      uint_fast32_t newsize)
{
    ring_buffer_free(rb);
    ring_buffer_init(rb, newsize);
}

/**
 * Check whether the buffer is full.
 *
 * @param rb Pointer to the ring buffer.
 * @return 1 if the buffer is full, otherwise 0.
 */
static inline int ring_buffer_is_full(ring_buffer_t * rb)
{
    return (rb->count == rb->size);
}

/** Check whether the buffer is empty. */
static inline int ring_buffer_is_empty(ring_buffer_t * rb)
{
    return (rb->count == 0);
}

/** Get number of elements in the buffer. */
static inline uint_fast32_t ring_buffer_count(ring_buffer_t * rb)
{
    return rb->count;
}

/** Get size of the ring buffer. */
static inline uint_fast32_t ring_buffer_size(ring_buffer_t * rb)
{
    return rb->size;
}

/**
 * Write data into the buffer.
 *
 * @param rb   The ring buffer handle.
 * @param src  The source array.
 * @param num  The the number of elements in the input buffer. Must be less
 *             than or equal to @ref rb->size.
 */
static inline void ring_buffer_write(ring_buffer_t * rb, unsigned char *src,
                                     uint_fast32_t num)
{
    if (!num)
        return;

    /* write pointer to first available slot */
    uint_fast32_t   wp = (rb->start + rb->count) % rb->size;

    /* new write pointer after successful write */
    uint_fast32_t   new_wp = (wp + num) % rb->size;

    if (new_wp > wp)
    {
        /* we can write in a single pass */
        memcpy(&rb->buffer[wp], src, num);
    }
    else
    {
        /* we need to wrap around the end */
        memcpy(&rb->buffer[wp], src, num - new_wp);
        memcpy(rb->buffer, &src[num - new_wp], new_wp);
    }

    /* Update count and check if overwrite has occurred, if yes, adjust
     * count and read pointer (rb->start)
     */
    rb->count += num;
    if (rb->count > rb->size)
    {
        rb->count = rb->size;
        rb->start = new_wp;
    }
}

/**
 * Read data from the riung buffer.
 *
 * @param  rb    The ring buffer handle.
 * @param  dest  Pointer to the preallocated destination buffer.
 * @param  num   The number of elements to read. Must be less than or equal to
 *               @ref rb->count.
 */
static inline void ring_buffer_read(ring_buffer_t * rb, unsigned char *dest,
                                    uint_fast32_t num)
{
    if (!num)
        return;

    /* index of the last value to read */
    uint_fast32_t   end = (rb->start + num - 1) % rb->size;

    if (end < rb->start)
    {
        /* we need to read in two passes */
        uint_fast32_t   split = rb->size - rb->start;

        memcpy(dest, &rb->buffer[rb->start], split);
        memcpy(&dest[split], rb->buffer, end + 1);
    }
    else
    {
        /* can read in a single pass */
        memcpy(dest, &rb->buffer[rb->start], num);
    }

    rb->count -= num;
    rb->start = (end + 1) % rb->size;
}


/**
 * Clear the buffer.
 *
 * @param rb The ring buffer handle.
 *
 * This function will clear the buffer by setting the start and the count to 0.
 */
static inline void ring_buffer_clear(ring_buffer_t * rb)
{
    rb->start = 0;
    rb->count = 0;
}

#endif // __RING_BUFFER_H__
