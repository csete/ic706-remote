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
 * @rxb             Ring buffer for storing incoming audio data.
 * @txb             Ring buffer for storing outgoing audio data.
 * @frames_tot      Total number of frames received.
 * @frames_avg      Average number of frames received per period.
 * @status_errors   Status errors received in the callback function.
 * @overflows       Number of times we wrote incoming audio data into a full
 *                  buffer
 * @underflows      Number of times audio output requested more frames than we
 *                  had in the buffer.
 * @conf            Audio configuration flags (input, output duplex).
 */
struct audio_data {
    PaStream       *stream;
    const PaDeviceInfo *device_info;
    PaStreamParameters input_param;

    ring_buffer_t  *rxb;
    ring_buffer_t  *txb;

    uint64_t        frames_tot;
    uint32_t        frames_avg;
    uint32_t        status_errors;
    uint32_t        overflows;
    uint32_t        underflows;
    uint8_t         conf;
};

typedef struct audio_data audio_t;

#define AUDIO_CONF_INPUT    0x01
#define AUDIO_CONF_OUTPUT   0x02
#define AUDIO_CONF_DUPLEX   0x03

/**
 * Initialize audio backend.
 * @param   index   The index of the audio device to initialize.
 * @param   sample_rate Sample rate. Use 0 for default.
 * @param   conf    Audio configuration, see AUDIO_CONF_xyz.
 * @return  Pointer to the audio handle to be used for subsequent API calls.
 * @sa      audio_list_devices()
 * @note    If audio is initialized for both input and output, it will run in
 *          full duplex mode, i.e. the callback will both read and write
 *          samples in the same call.
 */
audio_t        *audio_init(int index, uint32_t sample_rate, uint8_t conf);

/**
 * Close audio stream and terminate portaudio session.
 * @param audio The audio handle.
 * @return The error code returned by portaudio (0 means OK).
 */
int             audio_close(audio_t * audio);

/**
 * Start audio stream for reading.
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
uint32_t        audio_read_frames(audio_t * audio, unsigned char *buffer,
                                  uint32_t frames);

/**
 * Write audio frames
 * @param   audio   Pointer to the audio handle.
 * @param   buffer  Pointer to the buffer containing the frames to write.
 * @param   frames  The number of frames to write.
 */
void            audio_write_frames(audio_t * audio, uint8_t * buffer,
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
