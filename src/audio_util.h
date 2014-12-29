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

/**
 * Data structure for audio configuration and data.
 * 
 * @stream          Audio stream handle.
 * @device_info     Audio device info.
 * @input_param     Input parameters.
 * @frames_avg      Average number of frames received per period.
 * @frames_tot      Total number of frames received.
 */
struct audio_data {
    PaStream       *stream;
    const PaDeviceInfo *device_info;
    PaStreamParameters input_param;

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
 * List available audio devices.
 * 
 * @return The number of available audio devices or the portaudio error code.
 *
 * @note This function will start with initializing portaudio and finish with
 *       terminating portaudio.
 */
int             audio_list_devices(void);

#endif
