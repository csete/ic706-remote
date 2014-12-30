/*
 * Copyright (c) 2014, Alexandru Csete
 * All rights reserved.
 *
 * This software is licensed under the terms and conditions of the
 * Simplified BSD License. See license.txt for details.
 *
 */
#include <inttypes.h>           // PRId64 and PRIu64
#include <portaudio.h>
#include <stdio.h>
#include <stdlib.h>

#include "audio_util.h"


#define SAMPLE_RATE 48000
#define CHANNELS    1
#define FRAME_SIZE  2 * CHANNELS        /* 2 bytes / sample */
#define BUFFER_LEN_SEC 0.1
#define BUFFER_SIZE (SAMPLE_RATE * FRAME_SIZE) * BUFFER_LEN_SEC


int audio_reader_cb(const void *input, void *output, unsigned long frame_cnt,
                    const PaStreamCallbackTimeInfo * timeInfo,
                    PaStreamCallbackFlags statusFlags, void *user_data)
{
    (void)output;
    (void)timeInfo;

    audio_t        *audio = (audio_t *) user_data;
    PaStreamCallbackResult result = paContinue;
    unsigned long   byte_cnt = frame_cnt * FRAME_SIZE;

    if (byte_cnt + ring_buffer_count(audio->rb) > ring_buffer_size(audio->rb))
        audio->overflows++;

    ring_buffer_write(audio->rb, (unsigned char *)input, byte_cnt);

    audio->frames_tot += frame_cnt;

    if (audio->frames_avg)
        audio->frames_avg = (audio->frames_avg + frame_cnt) / 2;
    else
        audio->frames_avg = frame_cnt;

    if (statusFlags)
        audio->status_errors++;

    return result;
}

audio_t        *audio_init(int index)
{
    audio_t        *audio;
    PaError         error;
    int             input_rate = SAMPLE_RATE;


    error = Pa_Initialize();  /** FIXME: make it quiet */
    if (error != paNoError)
    {
        fprintf(stderr, "Error initializing audio %d: %s", error,
                Pa_GetErrorText(error));
        return NULL;
    }

    audio = (audio_t *) malloc(sizeof(audio_t));
    if (!audio)
        return NULL;

    if (index < 0)
    {
        audio->input_param.device = Pa_GetDefaultInputDevice();
        fprintf(stderr, "Audio device not specified. Default is %d\n",
                audio->input_param.device);
    }
    else
    {
        audio->input_param.device = index;
    }

    /** FIXME: ring buffer assumes 1 channel */
    audio->input_param.channelCount =
        (audio->device_info->maxInputChannels == 1) ? 1 : 2;
    fprintf(stderr, "Number of channels: %d\n",
            audio->input_param.channelCount);

    audio->input_param.sampleFormat = paInt16;
    audio->input_param.hostApiSpecificStreamInfo = NULL;
    audio->input_param.suggestedLatency = 0.01f;        //audio->device_info->defaultLowInputLatency;

    audio->device_info = Pa_GetDeviceInfo(audio->input_param.device);
    fprintf(stderr, "Using audio device no. %d: %s\n",
            audio->input_param.device, audio->device_info->name);

    /** FIXME: check if sample rate is supported */
    if (input_rate == 0)
        input_rate = audio->device_info->defaultSampleRate;
    fprintf(stderr, "Sample rate: %d\n", input_rate);

    fprintf(stderr, "Latencies (LH): %d  %.d\n",
            (int)(1.e3 * audio->device_info->defaultLowInputLatency),
            (int)(1.e3 * audio->device_info->defaultHighInputLatency));

    error = Pa_OpenStream(&audio->stream, &audio->input_param, NULL,
                          input_rate, paFramesPerBufferUnspecified,
                          paClipOff | paDitherOff, audio_reader_cb, audio);

    if (error != paNoError)
    {
        fprintf(stderr, "Error opening audio stream %d (%s)\n", error,
                Pa_GetErrorText(error));

        free(audio);
        return NULL;
    }

    /* allocate ring buffer */
    audio->rb = (ring_buffer_t *) malloc(sizeof(ring_buffer_t));
    ring_buffer_init(audio->rb, BUFFER_SIZE);

    fprintf(stderr, "Audio stream opened\n");

    return audio;
}


int audio_close(audio_t * audio)
{
    PaError         error;

    error = Pa_CloseStream(audio->stream);
    if (error != paNoError)
        fprintf(stderr, "Error closing audio stream %d: %s\n",
                error, Pa_GetErrorText(error));
    else
        fprintf(stderr, "Stream closed\n");

    Pa_Terminate();

    fprintf(stderr, " Audio frames (tot): %" PRIu64 "\n", audio->frames_tot);
    fprintf(stderr, " Audio frames (avg): %" PRIu32 "\n", audio->frames_avg);
    fprintf(stderr, " Status errors:      %" PRIu32 "\n",
            audio->status_errors);
    fprintf(stderr, " Buffer overflows:   %" PRIu32 "\n", audio->overflows);

    ring_buffer_free(audio->rb);
    free(audio->rb);
    free(audio);

    return error;
}

int audio_start(audio_t * audio)
{
    PaError         error;

    audio->status_errors = 0;
    audio->overflows = 0;
    audio->frames_avg = 0;
    audio->frames_tot = 0;

    ring_buffer_clear(audio->rb);

    error = Pa_StartStream(audio->stream);
    if (error != paNoError)
        fprintf(stderr, "Error starting audio stream %d: %s\n",
                error, Pa_GetErrorText(error));
    else
        fprintf(stderr, "Audio stream started\n");

    return error;
}

int audio_stop(audio_t * audio)
{
    PaError         error = 0;

    if (Pa_IsStreamActive(audio->stream))
    {
        error = Pa_StopStream(audio->stream);
        if (error != paNoError)
            fprintf(stderr, "Error stopping audio stream %d: %s\n",
                    error, Pa_GetErrorText(error));
        else
            fprintf(stderr, "Audio stream stopped\n");
    }
    else
    {
        fprintf(stderr, "Audio stream not active\n");
    }

    return error;
}

uint32_t audio_frames_available(audio_t * audio)
{
    return ring_buffer_count(audio->rb) / FRAME_SIZE;
}

uint32_t audio_get_frames(audio_t * audio, unsigned char *buffer,
                          uint32_t frames)
{
    if (ring_buffer_count(audio->rb) < frames)
        frames = ring_buffer_count(audio->rb);

    ring_buffer_read(audio->rb, buffer, frames);

    return frames;
}


int audio_list_devices(void)
{
    const PaDeviceInfo *dev_info;
    PaError         error;
    int             i, num_devices;


    error = Pa_Initialize();  /** FIXME: make it quiet */
    if (error != paNoError)
    {
        fprintf(stderr, "Error initializing audio %d: %s",
                error, Pa_GetErrorText(error));
        return 0;
    }

    num_devices = Pa_GetDeviceCount();
    if (num_devices < 0)
    {
        fprintf(stderr, "ERROR: Pa_GetDeviceCount returned 0x%x\n",
                num_devices);
        Pa_Terminate();
        return 0;
    }

    fprintf(stderr, "\nAvailable input devices:\n");
    fprintf(stderr, " IDX  Ichan  Rate   Lat. (ms)  Name\n");
    for (i = 0; i < num_devices; i++)
    {
        dev_info = Pa_GetDeviceInfo(i);

        if (dev_info->maxInputChannels > 0)
        {
            fprintf(stderr, " %2d   %3d  %7.0f  %3.0f  %3.0f   %s\n",
                    i, dev_info->maxInputChannels, dev_info->defaultSampleRate,
                    1.e3 * dev_info->defaultLowInputLatency,
                    1.e3 * dev_info->defaultHighInputLatency, dev_info->name);
        }
    }

    fprintf(stderr, "\n");

    Pa_Terminate();

    return num_devices;
}
