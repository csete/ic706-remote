/*
 * Copyright (c) 2014, Alexandru Csete
 * All rights reserved.
 *
 * This software is licensed under the terms and conditions of the
 * Simplified BSD License. See license.txt for details.
 *
 */
#include <portaudio.h>
#include <stdio.h>
#include <stdlib.h>

#include "audio_util.h"


audio_t        *audio_init(int index)
{
    audio_t        *audio;
    PaError         error;
    int             input_rate = 48000;


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

    audio->frames_avg = 0;
    audio->frames_tot = 0;

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

    /** FIXME: check if sample rate is supproted */
    if (input_rate == 0)
        input_rate = audio->device_info->defaultSampleRate;
    fprintf(stderr, "Sample rate: %d\n", input_rate);

    fprintf(stderr, "Latencies (LH): %d  %.d\n",
            (int)(1.e3 * audio->device_info->defaultLowInputLatency),
            (int)(1.e3 * audio->device_info->defaultHighInputLatency));

    error = Pa_OpenStream(&audio->stream, &audio->input_param, NULL,
                          input_rate, paFramesPerBufferUnspecified,
                          paClipOff | paDitherOff, NULL, NULL);
    //audio_reader_cb, audio);

    if (error != paNoError)
    {
        fprintf(stderr, "Error opening audio stream %d (%s)\n", error,
                Pa_GetErrorText(error));

        free(audio);
        return NULL;
    }

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

    free(audio);

    return error;
}

int audio_start(audio_t * audio)
{
    PaError         error;

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
