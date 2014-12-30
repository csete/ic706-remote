/*
 * Copyright (c) 2014, Alexandru Csete
 * All rights reserved.
 *
 * This software is licensed under the terms and conditions of the
 * Simplified BSD License. See license.txt for details.
 *
 */
#ifndef __AUDIO_UTIL_H__
#define __AUDIO_UTIL_H__

#include <portaudio.h>
#include <stdint.h>

#include "ring_buffer.h"

/**
 * Data structure for audio configuration and data.
 * 
 * @stream          Audio stream handle.
 * @device_info     Audio device info.
 * @input_param     Input parameters.
 * @rb              Ring buffer for storing audio data.
 * @status_errors   Status errors received in the callback function.
 * @overflows       Number of times we wrote while buffer was already full.
 * @frames_avg      Average number of frames received per period.
 * @frames_tot      Total number of frames received.
 */
struct audio_data {
    PaStream       *stream;
    const PaDeviceInfo *device_info;
    PaStreamParameters input_param;

    ring_buffer_t  *rb;

    uint32_t        status_errors;
    uint32_t        overflows;
    uint32_t        frames_avg;
    uint64_t        frames_tot;
};

typedef struct audio_data audio_t;

/**
 * Initialize audio backend.
 * @param   index   The index of the audio device to initialize.
 * @return  Pointer to the audio handle to be used for subsequent API calls.
 * @sa audio_list_devices()
 */
audio_t        *audio_init(int index);

/**
 * Close audio stream and terminate portaudio session.
 * @param audio The audio handle.
 * @return The error code returned by portaudio (0 means OK).
 */
int             audio_close(audio_t * audio);

/**
 * Start audio stream.
 * @param audio The audio handle.
 * @return  The error code returned by portaudio (0 means OK).
 */
int             audio_start(audio_t * audio);

/**
 * Stop audio stream.
 * @param audio The audio handle.
 * @return The error code returned by portaudio (0 means OK).
 */
int             audio_stop(audio_t * audio);

/**
 * Get number of audio frames available for read
 * @param audio Pointer to the audio handle.
 * @return The number of frames available in the buffer.
 *
 * A frame is one audio unit sample, e.g. 2 bytes for 1 channel S16 sample
 * format, 8 bytes for 2 channel S32 sample format.
 */
uint32_t        audio_frames_available(audio_t * audio);

/**
 * Read audio frames.
 * @param   audio     Pointer to the audio handle.
 * @param   buffer    Pointer to the buffer where the data shouldbe copied.
 * @param   frames    The number of frames to copy.
 * @return  The umber of frames actually copied to the buffer.
 */
uint32_t        audio_get_frames(audio_t * audio, unsigned char *buffer,
                                 uint32_t frames);
/**
 * List available audio devices.
 * 
 * @return The number of available audio devices or the portaudio error code.
 *
 * @note This function will start with initializing portaudio and finish with
 *       terminating portaudio.
 */
int             audio_list_devices(void);

#endif
