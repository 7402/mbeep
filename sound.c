//
// sound.c
// mbeep
//
// Copyright (C) 2018 Michael Budiansky. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are permitted
// provided that the following conditions are met:
//
// Redistributions of source code must retain the above copyright notice, this list of conditions
// and the following disclaimer.
//
// Redistributions in binary form must reproduce the above copyright notice, this list of conditions
// and the following disclaimer in the documentation and/or other materials provided with the
// distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
// FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
// WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __APPLE__
    #include <OpenAL/al.h>
    #include <OpenAL/alc.h>
#else
    #include <AL/al.h>
    #include <AL/alc.h>
    #define M_PI 3.14159265358979323846
#endif

#include "sound.h"

#define SAMPLES_PER_SECOND 44100
#define BUFFER_SIZE (1 * SAMPLES_PER_SECOND)
#define RAMP_MSEC 5.0

bool init_OK = false;
ALCdevice *device = NULL;
ALCcontext *context = NULL;

#define NUM_BUFFERS 3
ALuint buffers[NUM_BUFFERS];
ALshort *data = NULL;
bool buffer_queued[NUM_BUFFERS];
int current_buffer = 0;
size_t data_offset = 0;
bool buffers_OK = false;

ALuint source;
bool source_OK = false;

SoundError al_to_se_error(ALenum al_error);
SoundError al_to_se_error(ALenum al_error)
{
    switch (al_error) {
        case AL_NO_ERROR:           return(SE_NO_ERROR);
        case AL_INVALID_NAME:       return(SE_INVALID_NAME);
        case AL_INVALID_ENUM:       return(SE_INVALID_ENUM);
        case AL_INVALID_VALUE:      return(SE_INVALID_VALUE);
        case AL_INVALID_OPERATION:  return(SE_INVALID_OPERATION);
        case AL_OUT_OF_MEMORY:      return(SE_OUT_OF_MEMORY);
        default:                    return(SE_UNKNOWN);
    }
}

SoundError init_sound(void)
{
    SoundError error = SE_NO_ERROR;
    alGetError();

    for (int k = 0; k < NUM_BUFFERS; k++) {
        buffer_queued[k] = false;
    }

    device = alcOpenDevice(NULL);
    if (device == NULL) error = SE_NO_DEVICE;

    if (error == SE_NO_ERROR) {
        context = alcCreateContext(device, NULL);
        if (device == NULL) error = SE_NO_CONTEXT;
    }

    if (error == SE_NO_ERROR) {
        alcMakeContextCurrent(context);
        error = al_to_se_error(alGetError());
    }

    if (error == SE_NO_ERROR) {
        alGenBuffers(NUM_BUFFERS, buffers);
        error = al_to_se_error(alGetError());
        buffers_OK = error == SE_NO_ERROR;
    }

    if (buffers_OK) {
        data = (ALshort *)calloc(BUFFER_SIZE, sizeof(ALshort));
        if (data == NULL) error = SE_OUT_OF_MEMORY;
    }

    if (error == SE_NO_ERROR) {
        alGenSources(1, &source);
        error = al_to_se_error(alGetError());
        source_OK = error == SE_NO_ERROR;
    }

    return error;
}

SoundError fill_buffer_or_file(double freq, double msec, FILE *file)
{
    SoundError error = SE_NO_ERROR;

    if (file != NULL) {
        error = fill_file(freq, msec, file);

    } else {
        error = fill_buffer(freq, msec);
    }

    return error;
}

SoundError fill_buffer(double freq, double msec)
{
    SoundError error = SE_NO_ERROR;

    // total - total number of samples
    // count - samples remaining to be written
    // index - offset into sequence of samples
    size_t total = (size_t)(0.001 * msec * SAMPLES_PER_SECOND);
    size_t count = total;
    size_t index = 0;

    // To prevent clicks at beginning and end of tone, ramp amplitude up at beginning and down
    // at end by multiplying by sine / cosine function. Use RAMP_MSEC or 10% of duration,
    // whichever is smaller.
    double max_ramp_msec = msec / 10.0;
    double ramp_msec = RAMP_MSEC < max_ramp_msec ? RAMP_MSEC : max_ramp_msec;
    size_t ramp = (size_t)(0.001 * ramp_msec * SAMPLES_PER_SECOND);

    while (count > 0 && error == SE_NO_ERROR) {
        while (buffer_queued[current_buffer] && error == SE_NO_ERROR) {
            // if current buffer is already queued, then they are all queued; need to wait to until
            // current buffer (which is the oldest queued buffer) is done, so we can start
            // filling it again
            ALint processed = 0;
            while (processed == 0 && error == SE_NO_ERROR) {
                alGetSourcei(source, AL_BUFFERS_PROCESSED, &processed);
                error = al_to_se_error(alGetError());
            }

            if (error == SE_NO_ERROR) {
                alSourceUnqueueBuffers(source, 1, &buffers[current_buffer]);
                error = al_to_se_error(alGetError());
#if DEBUG
                printf("[%d] unqueue %d\n", processed, current_buffer);
#endif
            }

            if (error == SE_NO_ERROR) {
                alGetSourcei(source, AL_BUFFERS_PROCESSED, &processed);
#if DEBUG
                printf("[%d]\n", processed);
#endif
                buffer_queued[current_buffer] = false;
            }
        }

        if (error == SE_NO_ERROR) {
            // available - space remaining in current buffer
            size_t available = BUFFER_SIZE - data_offset;
            // samples - number of samples to write during this cycle through loop
            size_t samples = count <= available ? count : available;

#if DEBUG
            printf("fill buffer %d with %ld samples at %ld (%.1f)\n", current_buffer, (long)samples,
                   (long)data_offset, freq);
#endif

            write_data(data + data_offset, freq, ramp, total, index, samples);

            count -= samples;
            index += samples;
            data_offset += samples;

            if (data_offset == BUFFER_SIZE) {
                // buffer is full; write it out
                alBufferData(buffers[current_buffer], AL_FORMAT_MONO16, data,
                             (ALsizei)BUFFER_SIZE * sizeof(ALshort), SAMPLES_PER_SECOND);

                error = al_to_se_error(alGetError());
#if DEBUG
                printf("queue %d\n", current_buffer);
#endif

                if (error == SE_NO_ERROR) {
                    // queue current buffer
                    alSourceQueueBuffers(source, 1, &buffers[current_buffer]);
                    buffer_queued[current_buffer] = true;
                    error = al_to_se_error(alGetError());
                }

                if (error == SE_NO_ERROR) {
                    ALint state;
                    alGetSourcei(source, AL_SOURCE_STATE, &state);
                    if (state != AL_PLAYING) {
                        // nothing is playing; either we haven't started yet, or we finished all
                        // queued buffers.
                        
                        // make sure all except current buffer are unqueued
                        for (int k = 0; k < NUM_BUFFERS && error == SE_NO_ERROR; k++) {
                            if (k != current_buffer && buffer_queued[k]) {
                                alSourceUnqueueBuffers(source, 1, &buffers[k]);
                                error = al_to_se_error(alGetError());
#if DEBUG
                                printf("unqueue non-playing %d\n", k);
#endif
                                buffer_queued[k] = false;
                            }
                        }

                        if (error == SE_NO_ERROR) {
                            alSourcePlay(source);
#if DEBUG
                            printf("play\n");
#endif
                            error = al_to_se_error(alGetError());
                        }
                    }
                }

                current_buffer = (current_buffer + 1) % NUM_BUFFERS;
                data_offset = 0;
            }
        }
    }

    return error;
}

#define WAVE_HEADER_SIZE 44

// fill .wav file header area with zeroes
SoundError begin_wave_file(FILE *file)
{
    SoundError error = SE_NO_ERROR;
    uint8_t header[WAVE_HEADER_SIZE];

    memset(header, 0, WAVE_HEADER_SIZE);
    fwrite(&header, WAVE_HEADER_SIZE, 1, file);

    return error;
}

#define CHANNELS 1
#define BITS_PER_SAMPLE 16

// fill in .wav file header area
SoundError finish_wave_file(FILE *file)
{
    SoundError error = SE_NO_ERROR;

    struct {
        char label[4];
        uint32_t file_size_minus_8;
        char file_type[4];
        char marker[4];
        uint32_t length_so_far;
        uint16_t format_type;
        uint16_t channels;
        uint32_t samples_per_second;
        uint32_t bytes_per_second;
        uint16_t bytes_per_sample;
        uint16_t bits_per_sample_per_channel;
        char data_header[4];
        uint32_t data_size;
    } header;

    long offset = ftell(file);

    strncpy(header.label, "RIFF", 4);
    header.file_size_minus_8 = (uint32_t)offset - 8;
    strncpy(header.file_type, "WAVE", 4);
    strncpy(header.marker, "fmt ", 4);
    header.length_so_far = 16;
    header.format_type = 1;
    header.channels = CHANNELS;
    header.samples_per_second = SAMPLES_PER_SECOND;
    header.bytes_per_second = (SAMPLES_PER_SECOND * BITS_PER_SAMPLE * CHANNELS) / 8;
    header.bytes_per_sample = (BITS_PER_SAMPLE * CHANNELS) / 8;
    header.bits_per_sample_per_channel = 16;
    strncpy(header.data_header, "data", 4);
    header.data_size = (uint32_t)offset - WAVE_HEADER_SIZE;

    // overwrite header area at beginning
    fseek(file, 0L, SEEK_SET);
    fwrite(&header, sizeof(header), 1, file);
    
    // go back to end of file
    fseek(file, offset, SEEK_SET);
    fclose(file);

    return error;
}

#define BUF_SIZE 4096

// write .wav file data
SoundError fill_file(double freq, double msec, FILE *file)
{
    SoundError error = SE_NO_ERROR;

    size_t total = (size_t)(0.001 * msec * SAMPLES_PER_SECOND);
    size_t remaining = total;
    double max_ramp_msec = msec / 10.0;
    double ramp_msec = RAMP_MSEC < max_ramp_msec ? RAMP_MSEC : max_ramp_msec;
    size_t ramp = (size_t)(0.001 * ramp_msec * SAMPLES_PER_SECOND);
    size_t index = 0;

    int16_t buffer[BUF_SIZE];

    while (remaining > 0 && error == SE_NO_ERROR) {
        size_t count = remaining < BUF_SIZE ? remaining : BUF_SIZE;

        write_data(buffer,  freq, ramp, total, index, count);
        fwrite(buffer, sizeof(int16_t), count, file);

        index += count;
        remaining -= count;
    }

    return error;
}

// write fragment of sample data into buffer
void write_data(short *data_ptr, double freq, size_t ramp_count, size_t total_count,
                size_t start_index, size_t sample_count)
{
    if (freq == 0.0) {
        memset(data_ptr, 0, sample_count * sizeof(ALshort));

    } else {
        for (size_t k = start_index; k < start_index + sample_count; k++) {
            double theta = 2.0 * M_PI * freq * k / SAMPLES_PER_SECOND;
            double amplitude = sin(theta) * 32767;
            if (k < ramp_count) {
                amplitude *= sin(0.5 * M_PI * k / ramp_count);

            } else if (k > total_count - ramp_count) {
                amplitude *= sin(0.5 * M_PI * (total_count - k) / ramp_count);
            }

            *data_ptr++ = (ALshort)(amplitude);
        }
    }
}

// start playing data in buffers
SoundError play_buffers(void)
{
    SoundError error = SE_NO_ERROR;

    if (data_offset > 0) {
        // current buffer is partially filled
        alBufferData(buffers[current_buffer], AL_FORMAT_MONO16, data,
                     (ALsizei)data_offset * sizeof(ALshort), SAMPLES_PER_SECOND);

        error = al_to_se_error(alGetError());

        if (error == SE_NO_ERROR) {
            alSourceQueueBuffers(source, 1, &buffers[current_buffer]);
            buffer_queued[current_buffer] = true;
            error = al_to_se_error(alGetError());
        }

#if DEBUG
        printf("play_buffers queue %d\n", current_buffer);
#endif

        if (error == SE_NO_ERROR) {
            ALint state;
            alGetSourcei(source, AL_SOURCE_STATE, &state);
            if (state != AL_PLAYING) {
                // nothing is playing; either we haven't started yet, or we finished all
                // queued buffers.

                // make sure all except current buffer are unqueued
                for (int k = 0; k < NUM_BUFFERS && error == SE_NO_ERROR; k++) {
                    if (k != current_buffer && buffer_queued[k]) {
                        alSourceUnqueueBuffers(source, 1, &buffers[k]);
                        error = al_to_se_error(alGetError());
#if DEBUG
                        printf("play_buffers unqueue non-playing %d\n", k);
#endif
                        buffer_queued[k] = false;
                    }
                }

                if (error == SE_NO_ERROR) {
                    alSourcePlay(source);
                    error = al_to_se_error(alGetError());
#if DEBUG
                    printf("play_buffers play\n");
#endif
                }
            }
        }

        current_buffer = (current_buffer + 1) % NUM_BUFFERS;
        data_offset = 0;
    }

    return error;
}

// if any buffers are playing, wait until playing stops
SoundError wait_for_buffers(void)
{
    SoundError error = SE_NO_ERROR;

    bool done = false;
    while (!done && error == SE_NO_ERROR) {
        ALint value;
        alGetSourcei(source, AL_SOURCE_STATE, &value);
        error = al_to_se_error(alGetError());
        done = value != AL_PLAYING;
    }

    return error;
}

void close_sound(void)
{
    if (data != NULL) {
        free(data);
        data = NULL;
    }

    if (source_OK) {
        alDeleteSources(1, &source);
        source_OK = false;
    }

    if (buffers_OK) {
        alDeleteBuffers(NUM_BUFFERS, buffers);
        buffers_OK = false;
    }

    if (context != NULL) {
        alcMakeContextCurrent(NULL);
        alcDestroyContext(context);
        context = NULL;
    }

    if (device != NULL) {
        alcCloseDevice(device);
        device = NULL;
    }

    init_OK = false;
}


