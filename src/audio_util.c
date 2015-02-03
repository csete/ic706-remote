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
#define BUFFER_LEN_SEC 0.48
#define BUFFER_SIZE (SAMPLE_RATE * FRAME_SIZE) * BUFFER_LEN_SEC

/* buffer threshold to start playback */
#define PLAYBACK_THRESHOLD (SAMPLE_RATE * FRAME_SIZE) * 0.2


int audio_reader_cb(const void *input, void *output, unsigned long frame_cnt,
                    const PaStreamCallbackTimeInfo * timeInfo,
                    PaStreamCallbackFlags statusFlags, void *user_data)
{
    (void)output;
    (void)timeInfo;

    audio_t        *audio = (audio_t *) user_data;
    PaStreamCallbackResult result = paContinue;
    unsigned long   byte_cnt = frame_cnt * FRAME_SIZE;

    if (byte_cnt + ring_buffer_count(audio->rb) >
        ring_buffer_size(audio->rb))
    {
        audio->overflows++;
    }

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


int audio_writer_cb(const void *input, void *output, unsigned long frame_cnt,
                    const PaStreamCallbackTimeInfo * timeInfo,
                    PaStreamCallbackFlags statusFlags, void *user_data)
{
    (void)input;
    (void)timeInfo;

    audio_t        *audio = (audio_t *) user_data;
    PaStreamCallbackResult result = paContinue;
    unsigned long   byte_cnt = frame_cnt * FRAME_SIZE;
    unsigned long   i;
    uint16_t       *out = (uint16_t *)output;


    if (audio->player_state == AUDIO_STATE_BUFFERING)
    {
        if (ring_buffer_count(audio->rb) < PLAYBACK_THRESHOLD)
        {
            for (i = 0; i < frame_cnt; i++)
                out[i] = 0;

            return result;
        }
        /* there is enough data in buffer to start playback */
        audio->player_state = AUDIO_STATE_PLAYING;
    }


    if (byte_cnt > ring_buffer_count(audio->rb))
    {
        for (i = 0; i < frame_cnt; i++)
            out[i] = 0;

        audio->underflows++;
        fprintf(stderr, "aU: %ld / %" PRIuFAST32 "\n", byte_cnt,
                ring_buffer_count(audio->rb));

        /* switch back to buffering */
        audio->player_state = AUDIO_STATE_BUFFERING;
    }
    else
    {
        ring_buffer_read(audio->rb, (unsigned char *)output, byte_cnt);
        audio->frames_tot += frame_cnt;
    }

    /* update statistics */
    if (audio->frames_avg)
        audio->frames_avg = (audio->frames_avg + frame_cnt) / 2;
    else
        audio->frames_avg = frame_cnt;

    if (statusFlags)
        audio->status_errors++;

    return result;
}


audio_t        *audio_init(int index, uint32_t sample_rate, uint8_t conf)
{
    audio_t        *audio;
    PaError         error;

    if ((conf != AUDIO_CONF_INPUT) && (conf != AUDIO_CONF_OUTPUT))
    {
        fprintf(stderr, "%s: conf %d not implemented\n", __func__, conf);
        return NULL;
    }

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

    audio->frames_tot = 0;
    audio->frames_avg = 0;
    audio->status_errors = 0;
    audio->overflows = 0;
    audio->underflows = 0;
    audio->conf = conf;
    audio->player_state = AUDIO_STATE_STOPPED;

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
    audio->input_param.channelCount = CHANNELS;
    fprintf(stderr, "Number of channels: %d\n",
            audio->input_param.channelCount);

    audio->input_param.sampleFormat = paInt16;
    audio->input_param.hostApiSpecificStreamInfo = NULL;
    audio->input_param.suggestedLatency = 0.04f;        //audio->device_info->defaultLowInputLatency;

    audio->device_info = Pa_GetDeviceInfo(audio->input_param.device);
    fprintf(stderr, "Using audio device no. %d: %s\n",
            audio->input_param.device, audio->device_info->name);

    /** FIXME: check if sample rate is supported */
    if (sample_rate == 0)
        sample_rate = audio->device_info->defaultSampleRate;
    fprintf(stderr, "Sample rate: %d\n", sample_rate);

    fprintf(stderr, "Latencies (LH): %d  %.d\n",
            (int)(1.e3 * audio->device_info->defaultLowInputLatency),
            (int)(1.e3 * audio->device_info->defaultHighInputLatency));

    switch (conf)
    {
    case AUDIO_CONF_INPUT:
        error = Pa_OpenStream(&audio->stream, &audio->input_param, NULL,
                              sample_rate, paFramesPerBufferUnspecified,
                              paClipOff | paDitherOff, audio_reader_cb, audio);
        break;

    case AUDIO_CONF_OUTPUT:
        error = Pa_OpenStream(&audio->stream, NULL, &audio->input_param,
                              sample_rate, paFramesPerBufferUnspecified,
                              paClipOff | paDitherOff,
                              audio_writer_cb, audio);
                              //NULL, NULL);
        break;

    default:
        /* should never happen */
        fprintf(stderr, "%s: Invalid conf %d\n", __func__, conf);
        error = paInvalidFlag;
        break;
    }

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

    ring_buffer_free(audio->rb);
    free(audio->rb);
    free(audio);

    return error;
}

int audio_start(audio_t * audio)
{
    PaError         error;

    audio->frames_tot = 0;
    audio->frames_avg = 0;
    audio->status_errors = 0;
    audio->overflows = 0;
    audio->underflows = 0;

    ring_buffer_clear(audio->rb);

    error = Pa_StartStream(audio->stream);
    if (error != paNoError)
    {
        fprintf(stderr, "Error starting audio stream %d: %s\n",
                error, Pa_GetErrorText(error));
    }
    else
    {
        fprintf(stderr, "Audio stream started\n");
        audio->player_state = AUDIO_STATE_BUFFERING;
    }

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

    audio->player_state = AUDIO_STATE_STOPPED;

    fprintf(stderr, " Audio frames (tot): %" PRIu64 "\n", audio->frames_tot);
    fprintf(stderr, " Audio frames (avg): %" PRIu32 "\n", audio->frames_avg);
    fprintf(stderr, " Status errors:      %" PRIu32 "\n",
            audio->status_errors);
    fprintf(stderr, " Buffer overflows:   %" PRIu32 "\n", audio->overflows);
    fprintf(stderr, " Buffer underflows:  %" PRIu32 "\n", audio->underflows);

    return error;
}

uint32_t audio_frames_available(audio_t * audio)
{
    return ring_buffer_count(audio->rb) / FRAME_SIZE;
}

uint32_t audio_read_frames(audio_t * audio, unsigned char *buffer,
                           uint32_t frames)
{
    uint32_t        frames_read = ring_buffer_count(audio->rb) / FRAME_SIZE;

    if (frames_read > frames)
        frames_read = frames;

    ring_buffer_read(audio->rb, buffer, frames_read * FRAME_SIZE);

    return frames_read;
}

void audio_write_frames(audio_t * audio, uint8_t * buffer, uint32_t frames)
{
    ring_buffer_write(audio->rb, buffer, frames * FRAME_SIZE);
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

    fprintf(stderr, "\nAvailable input / output devices:\n");
    fprintf(stderr, " IDX  CHi CHo  Rate   Lat. (ms)  Name\n");
    for (i = 0; i < num_devices; i++)
    {
        dev_info = Pa_GetDeviceInfo(i);

        if (dev_info->maxInputChannels > 0)
        {
            fprintf(stderr, " %2d  %3d %3d %7.0f  %3.0f  %3.0f   %s\n",
                    i, dev_info->maxInputChannels, dev_info->maxOutputChannels,
                    dev_info->defaultSampleRate,
                    1.e3 * dev_info->defaultLowInputLatency,
                    1.e3 * dev_info->defaultHighInputLatency, dev_info->name);
        }
    }

    fprintf(stderr, "\n");

    Pa_Terminate();

    return num_devices;
}
