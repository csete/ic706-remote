/*
 * Copyright (c) 2014, Alexandru Csete <oz9aec@gmail.com>
 * All rights reserved.
 *
 * This software is licensed under the terms and conditions of the
 * Simplified BSD License. See license.txt for details.
 *
 */
#include <alsa/asoundlib.h>

/**
 * Open and configure audio device for input.
 * @param device The name of the audio device, such as hw:1
 * @returns Pointer to an ALSA PCM handle or NULL if and error uccurred.
 *
 * The audio device is configured with the following settings:
 *   - Sample rate: 48 kHz
 *   - Sample format: S16LE
 *   - Channels: 2
 */
snd_pcm_t * create_pcm_input(const char * device);

