/*
 * Copyright (c) 2014, Alexandru Csete <oz9aec@gmail.com>
 * All rights reserved.
 *
 * This software is licensed under the terms and conditions of the
 * Simplified BSD License. See license.txt for details.
 *
 */
#include <alsa/asoundlib.h>
#include <stdio.h>

#include "audio_util.h"


snd_pcm_t * create_pcm_input(const char * device)
{
    int err;
    unsigned int rate;
    snd_pcm_t *pcm;
    snd_pcm_hw_params_t *hw_params;

    if (!device)
        return NULL;

    if ((err = snd_pcm_open(&pcm, device, SND_PCM_STREAM_CAPTURE, 0)) < 0)
    {
        fprintf(stderr, "Cannot open audio device %s (%s)\n", device,
                snd_strerror(err));
        return NULL;
    }

    if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0)
    {
        fprintf(stderr, "Cannot allocate hardware parameters: %s\n",
                snd_strerror(err));
        snd_pcm_close(pcm);
        return NULL;
    }
             
    if ((err = snd_pcm_hw_params_any(pcm, hw_params)) < 0)
    {
        fprintf(stderr, "Cannot initialize hardware parameters: %s\n",
                snd_strerror(err));
        snd_pcm_hw_params_free(hw_params);
        snd_pcm_close(pcm);
        return NULL;
    }

    if ((err = snd_pcm_hw_params_set_access(pcm, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
    {
        fprintf(stderr, "Cannot set access type: %s\n", snd_strerror(err));
        snd_pcm_hw_params_free(hw_params);
        snd_pcm_close(pcm);
        return NULL;
    }

    if ((err = snd_pcm_hw_params_set_format(pcm, hw_params, SND_PCM_FORMAT_S16_LE)) < 0)
    {
        fprintf(stderr, "Cannot set sample format: %s\n", snd_strerror(err));
        snd_pcm_hw_params_free(hw_params);
        snd_pcm_close(pcm);
        return NULL;
    }

    rate = 48000;
    if ((err = snd_pcm_hw_params_set_rate_near(pcm, hw_params, &rate, 0)) < 0)
    {
        fprintf(stderr, "Cannot set sample rate: %s\n", snd_strerror(err));
        snd_pcm_hw_params_free(hw_params);
        snd_pcm_close(pcm);
        return NULL;
    }
    fprintf(stderr, "Sample rate: %d\n", rate);

    if ((err = snd_pcm_hw_params_set_channels(pcm, hw_params, 2)) < 0)
    {
        fprintf(stderr, "Cannot set channel count: %s\n", snd_strerror(err));
        snd_pcm_hw_params_free(hw_params);
        snd_pcm_close(pcm);
        return NULL;
    }

    if ((err = snd_pcm_hw_params(pcm, hw_params)) < 0)
    {
        fprintf(stderr, "Cannot set HW parameters: %s\n", snd_strerror(err));
        snd_pcm_hw_params_free(hw_params);
        snd_pcm_close(pcm);
        return NULL;
    }

    snd_pcm_hw_params_free(hw_params);

    if ((err = snd_pcm_prepare(pcm)) < 0)
    {
        fprintf(stderr, "Cannot prepare audio interface for use: %s\n",
                snd_strerror(err));
        snd_pcm_close(pcm);
        return NULL;
    }

    return pcm;
}

