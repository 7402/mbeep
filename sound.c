//
// sound.c
// mbeep
//
// Copyright (C) 2018-23 Michael Budiansky. All rights reserved.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __APPLE__
    // first deprecated in macOS 10.15 - OpenAL is deprecated in favor of AVAudioEngine
    // suppress warnings:
    #define OPENAL_DEPRECATED
    
    #include <OpenAL/al.h>
    #include <OpenAL/alc.h>
#else
    #ifdef GPIO
        #include "tiny_gpio.h"
    #else
        #include <AL/al.h>
        #include <AL/alc.h>
    #endif

    #define M_PI 3.14159265358979323846
#endif

#include "sound.h"

#define SAMPLES_PER_SECOND 44100
#define BUFFER_SIZE (1 * SAMPLES_PER_SECOND)
#define RAMP_MSEC 20.0

#ifdef GPIO
#define __USE_POSIX199309
#define __USE_XOPEN2K
#include <time.h>

#else
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
#endif

void write_data(short *data_ptr, double freq, size_t ramp_count, size_t total_count,
                size_t start_index, size_t sample_count);

SoundError init_sound(void)
{
    SoundError error = SE_NO_ERROR;

#ifdef GPIO
    if (gpioInitialise() == 0) {
    	gpioSetMode(GPIO, PI_OUTPUT);
    }

#else

    for (int k = 0; k < NUM_BUFFERS; k++) {
        buffer_queued[k] = false;
    }

    device = alcOpenDevice(NULL);
    if (device == NULL) error = SE_NO_DEVICE;

    if (error == SE_NO_ERROR) {
        context = alcCreateContext(device, NULL);
        if (context == NULL) error = SE_NO_CONTEXT;
    }

    if (error == SE_NO_ERROR) {
        alcMakeContextCurrent(context);
        error = al_to_se_error(alGetError());
    }

    if (error == SE_NO_ERROR) {
        alGetError();
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
#endif

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

#ifdef GPIO
#define LONG_1E9 1000000000L
    struct timespec ts;

    if (freq == 0) {
        ts.tv_sec = floor(0.001 * msec);
        ts.tv_nsec = floor((msec - 1000 * ts.tv_sec) * 1000000.0);
        nanosleep(&ts, NULL);

    } else {
        long nsec_per_half_cycle = (long)((1e9 / freq) / 2);
        long cycles = (long)(freq * msec / 1000);

        clock_gettime(CLOCK_MONOTONIC, &ts);

        for (long k = 0; k < cycles; k++) {
        	gpioWrite(GPIO, 1);

            ts.tv_nsec += nsec_per_half_cycle;
            ts.tv_sec += ts.tv_nsec / LONG_1E9;
            ts.tv_nsec = ts.tv_nsec % LONG_1E9;
            clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL);

        	gpioWrite(GPIO, 0);

            ts.tv_nsec += nsec_per_half_cycle;
            ts.tv_sec += ts.tv_nsec / LONG_1E9;
            ts.tv_nsec = ts.tv_nsec % LONG_1E9;
            clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL);
        }
    }


#else
    // total - total number of samples
    // count - samples remaining to be written
    // index - offset into sequence of samples
    size_t total = (size_t)(0.001 * msec * SAMPLES_PER_SECOND);
    size_t count = total;
    size_t index = 0;

    // To prevent clicks at beginning and end of tone, ramp amplitude up at beginning and down
    // at end by multiplying by sine / cosine function. Use RAMP_MSEC or 30% of duration,
    // whichever is smaller.
    double max_ramp_msec = msec * 0.30;
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
                fprintf(stderr, "[%d] unqueue %d\n", processed, current_buffer);
#endif
            }

            if (error == SE_NO_ERROR) {
                alGetSourcei(source, AL_BUFFERS_PROCESSED, &processed);
#if DEBUG
                fprintf(stderr, "[%d]\n", processed);
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
            fprintf(stderr, "fill buffer %d with %ld samples at %ld (%.1f Hz %f msec)\n", current_buffer, (long)samples,
                   (long)data_offset, freq, msec);
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
                fprintf(stderr, "queue %d\n", current_buffer);
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
                                fprintf(stderr, "unqueue non-playing %d\n", k);
#endif
                                buffer_queued[k] = false;
                            }
                        }

                        if (error == SE_NO_ERROR) {
                            alSourcePlay(source);
#if DEBUG
                            fprintf(stderr, "play\n");
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
#endif

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
    WaveHeader header;

    long offset = ftell(file);

    memcpy(header.label, "RIFF", 4);
    header.file_size_minus_8 = (uint32_t)offset - 8;
    memcpy(header.file_type, "WAVE", 4);
    memcpy(header.marker, "fmt ", 4);
    header.length_so_far = 16;
    header.format_type = 1;
    header.channels = CHANNELS;
    header.samples_per_second = SAMPLES_PER_SECOND;
    header.bytes_per_second = (SAMPLES_PER_SECOND * BITS_PER_SAMPLE * CHANNELS) / 8;
    header.bytes_per_sample = (BITS_PER_SAMPLE * CHANNELS) / 8;
    header.bits_per_sample_per_channel = 16;
    memcpy(header.data_header, "data", 4);
    header.data_size = (uint32_t)offset - WAVE_HEADER_SIZE;

    // overwrite header area at beginning
    fseek(file, 0L, SEEK_SET);
    fwrite(&header, sizeof(header), 1, file);

    // go back to end of file
    fseek(file, offset, SEEK_SET);

    // caller must close file!

    return error;
}

#define BUF_SIZE 4096

// write .wav file data
SoundError fill_file(double freq, double msec, FILE *file)
{
#if DEBUG
    fprintf(stderr, "fill_file(%f, %f)\n", freq, msec);
#endif

    SoundError error = SE_NO_ERROR;

    size_t total = (size_t)(0.001 * msec * SAMPLES_PER_SECOND);
    size_t remaining = total;
    double max_ramp_msec = msec * 0.30;
    double ramp_msec = RAMP_MSEC < max_ramp_msec ? RAMP_MSEC : max_ramp_msec;
    size_t ramp = (size_t)(0.001 * ramp_msec * SAMPLES_PER_SECOND);
    size_t index = 0;

    int16_t buffer[BUF_SIZE];

#if DEBUG
    fprintf(stderr, "  total = %ld\n", (long)total);
#endif

    while (remaining > 0 && error == SE_NO_ERROR) {
        size_t count = remaining < BUF_SIZE ? remaining : BUF_SIZE;

        write_data(buffer, freq, ramp, total, index, count);
        fwrite(buffer, sizeof(int16_t), count, file);
#if DEBUG
        fprintf(stderr, "  fwrite %ld\n", (long)count);
#endif

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
        memset(data_ptr, 0, sample_count * sizeof(short));

    } else {
        for (size_t k = start_index; k < start_index + sample_count; k++) {
            double theta = 2.0 * M_PI * freq * k / SAMPLES_PER_SECOND;
            double amplitude = sin(theta) * 32767;
            if (k < ramp_count) {
                amplitude *= sin(0.5 * M_PI * k / ramp_count);

            } else if (k > total_count - ramp_count) {
                amplitude *= sin(0.5 * M_PI * (total_count - k) / ramp_count);
            }

            *data_ptr++ = (short)(amplitude);
        }
    }
}

// start playing data in buffers
SoundError play_buffers(void)
{
    SoundError error = SE_NO_ERROR;

#ifndef GPIO
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
        fprintf(stderr, "play_buffers queue %d\n", current_buffer);
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
                        fprintf(stderr, "play_buffers unqueue non-playing %d\n", k);
#endif
                        buffer_queued[k] = false;
                    }
                }

                if (error == SE_NO_ERROR) {
                    alSourcePlay(source);
                    error = al_to_se_error(alGetError());
#if DEBUG
                    fprintf(stderr, "play_buffers play\n");
#endif
                }
            }
        }

        current_buffer = (current_buffer + 1) % NUM_BUFFERS;
        data_offset = 0;
    }
#endif

    return error;
}

bool sound_playing(void)
{
#ifndef GPIO

    ALint value;
    
    alGetSourcei(source, AL_SOURCE_STATE, &value);
    SoundError error = al_to_se_error(alGetError());
    
    bool playing = error == SE_NO_ERROR && value == AL_PLAYING;

    return playing;

#else
	return false;
#endif
}

// if any buffers are playing, wait until playing stops
SoundError wait_for_buffers(void)
{
    SoundError error = SE_NO_ERROR;

#if DEBUG
    fprintf(stderr, "wait_for_buffers()\n");
#endif
    
#ifndef GPIO
    bool done = false;
    while (!done && error == SE_NO_ERROR) {
        ALint value;
        alGetSourcei(source, AL_SOURCE_STATE, &value);
        error = al_to_se_error(alGetError());
        done = value != AL_PLAYING;
    }
    
    for (int k = 0; k < NUM_BUFFERS && error == SE_NO_ERROR; k++) {
        if (buffer_queued[k]) {
            alSourceUnqueueBuffers(source, 1, &buffers[k]);
            error = al_to_se_error(alGetError());
#if DEBUG
            fprintf(stderr, "wait_for_buffers unqueue %d\n", k);
#endif
            buffer_queued[k] = false;
        }
    }

#endif

#if DEBUG
    if (error != SE_NO_ERROR) fprintf(stderr, "wait_for_buffers() = %s\n", sound_error_text(error));
#endif

    return error;
}

void close_sound(void)
{
#ifndef GPIO
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
#endif
}

SoundError play_wav(const char *path)
{
#ifdef GPIO
    return SE_INVALID_OPTION;
    
#else
    WaveHeader header;
    int16_t *file_data = NULL;
    long file_size;
    
    SoundError error = read_wav(path, &header, &file_data, &file_size);

#if DEBUG
            fprintf(stderr, "wav file size %ld\n", file_size);
#endif

    if (error == SE_NO_ERROR) error = play_wav_data(&header, file_data, file_size);
    
    if (error == SE_NO_ERROR) error = wait_for_buffers();
    
    if (file_data != NULL) {
        free(file_data);
        file_data = NULL;
    }

#if DEBUG
    if (error != SE_NO_ERROR) fprintf(stderr, "play_wav() = %s\n", sound_error_text(error));
#endif

    return error;
#endif
}

SoundError read_wav(const char *path, WaveHeader *header, int16_t **file_data, long *file_size)
{
#ifdef GPIO
    return SE_INVALID_OPTION;
    
#else
    SoundError error = SE_NO_ERROR;
    FILE *file = fopen(path, "r");
    *file_data = NULL;
    
    if (file == NULL) error = SE_INPUT_FILE_OPEN_ERROR;
    
    if (error == SE_NO_ERROR) {
        fseek(file, 0L, SEEK_END);
        *file_size = ftell(file);
        rewind(file);

        if (*file_size < (long)sizeof(WaveHeader)) error = SE_INVALID_FILE_FORMAT;
    }
    
    if (error == SE_NO_ERROR) {
        if (1 != fread(header, sizeof(WaveHeader), 1, file)) error = SE_FILE_READ_ERROR;
    }
    
    if (error == SE_NO_ERROR) {
        if (header->file_size_minus_8 == 2147483684) {
            fprintf(stderr, "patching file sizes\n");
            
            header->file_size_minus_8 = (uint32_t)*file_size - 8;
            header->data_size = (uint32_t)*file_size - sizeof(WaveHeader);
        }
        
#if DEBUG
        /*
        struct WaveHeader {
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
        };
        */
        fprintf(stderr, "Header for %s\n", path);
        fprintf(stderr, "label = %c%c%c%c\n", header->label[0], header->label[1], header->label[2], header->label[3]);
        fprintf(stderr, "file_size_minus_8 = %u\n", header->file_size_minus_8);
        fprintf(stderr, "file_type = %c%c%c%c\n", header->file_type[0], header->file_type[1], header->file_type[2], header->file_type[3]);
        fprintf(stderr, "marker = %c%c%c%c\n", header->marker[0], header->marker[1], header->marker[2], header->marker[3]);
        fprintf(stderr, "length_so_far = %u\n", header->length_so_far);
        fprintf(stderr, "format_type = %d\n", (int)header->format_type);
        fprintf(stderr, "channels = %d\n", (int)header->channels);
        fprintf(stderr, "samples_per_second = %u\n", header->samples_per_second);
        fprintf(stderr, "bytes_per_second = %u\n", header->bytes_per_second);
        fprintf(stderr, "bytes_per_sample = %d\n", (int)header->bytes_per_sample);
        fprintf(stderr, "bits_per_sample_per_channel = %d\n", (int)header->bits_per_sample_per_channel);
        fprintf(stderr, "data_header = %c%c%c%c\n", header->data_header[0], header->data_header[1], header->data_header[2], header->data_header[3]);
        fprintf(stderr, "data_size = %u\n", header->data_size);

#endif
        
        bool ok = 0 == strncmp(header->label, "RIFF", 4);
        ok = ok && 0 == strncmp(header->file_type, "WAVE", 4);
        ok = ok && 0 == strncmp(header->marker, "fmt ", 4);
        ok = ok && 1 == header->format_type;
        ok = ok && 1 == header->channels;
//        ok = ok && SAMPLES_PER_SECOND == header->samples_per_second;
        ok = ok && 2 == header->bytes_per_sample;
        ok = ok && 1 == header->channels;
        ok = ok && 0 == strncmp(header->data_header, "data", 4);
        ok = ok && (long long)*file_size >= (long long)sizeof(WaveHeader) + (long long)header->data_size;

        if (!ok) error = SE_INVALID_FILE_FORMAT;
    }
    
    if (error == SE_NO_ERROR) {
        *file_data = (int16_t *)malloc((size_t)header->data_size);
        if (*file_data == NULL) error = SE_OUT_OF_MEMORY;
    }
    
    if (error == SE_NO_ERROR) {
        if (header->data_size != fread(*file_data, 1, header->data_size, file)) {
            error = SE_FILE_READ_ERROR;
        }
    }
     
    if (file != NULL) fclose(file);

#if DEBUG
    if (error != SE_NO_ERROR) fprintf(stderr, "read_wav() = %s\n", sound_error_text(error));
#endif

    return error;
#endif
}

SoundError play_wav_data(WaveHeader *header, int16_t *file_data, long file_size)
{
#ifdef GPIO
    return SE_INVALID_OPTION;
    
#else
    
#if 0
    SoundError error = SE_NO_ERROR;

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
            fprintf(stderr, "[%d] unqueue %d\n", processed, current_buffer);
#endif
        }

        if (error == SE_NO_ERROR) {
            alGetSourcei(source, AL_BUFFERS_PROCESSED, &processed);
#if DEBUG
            fprintf(stderr, "[%d]\n", processed);
#endif
            buffer_queued[current_buffer] = false;
        }
    }
#else
    SoundError error = wait_for_buffers();
#endif

    if (error == SE_NO_ERROR) {
        // total - total number of samples
        size_t total = header->data_size / header->bytes_per_sample;

        // write entire file data into buffer
        alBufferData(buffers[current_buffer], AL_FORMAT_MONO16, file_data,
                     (ALsizei)total * sizeof(ALshort), header->samples_per_second);

        error = al_to_se_error(alGetError());
#if DEBUG
        fprintf(stderr, "queue %d\n", current_buffer);
#endif

        if (error == SE_NO_ERROR) {
            // queue buffer
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
                        fprintf(stderr, "unqueue non-playing %d\n", k);
#endif
                        buffer_queued[k] = false;
                    }
                }

                if (error == SE_NO_ERROR) {
                    alSourcePlay(source);
#if DEBUG
                    fprintf(stderr, "play\n");
#endif
                    error = al_to_se_error(alGetError());
                }
            }
        }

        current_buffer = (current_buffer + 1) % NUM_BUFFERS;
        data_offset = 0;
    }

#if DEBUG
    if (error != SE_NO_ERROR) fprintf(stderr, "play_wav_data() = %s\n", sound_error_text(error));
#endif

    return error;
#endif
}

const char *sound_error_text(SoundError error)
{
    switch(error) {
        case SE_NO_ERROR:           return "SE_NO_ERROR";           break;
        case SE_EXIT:               return "SE_EXIT";               break;
        case SE_NO_DEVICE:          return "SE_NO_DEVICE";          break;
        case SE_NO_CONTEXT:         return "SE_NO_CONTEXT";         break;
        case SE_UNKNOWN:            return "SE_UNKNOWN";            break;
        case SE_INVALID_NAME:       return "SE_INVALID_NAME";       break;
        case SE_INVALID_ENUM:       return "SE_INVALID_ENUM";       break;
        case SE_INVALID_VALUE:      return "SE_INVALID_VALUE";      break;
        case SE_INVALID_OPERATION:  return "SE_INVALID_OPERATION";  break;
        case SE_OUT_OF_MEMORY:      return "SE_OUT_OF_MEMORY";      break;
        case SE_INVALID_FREQUENCY:  return "SE_INVALID_FREQUENCY";  break;
        case SE_INVALID_WPM:        return "SE_INVALID_WPM";        break;
        case SE_INVALID_BPM:        return "SE_INVALID_BPM";        break;
        case SE_INVALID_MIDI:       return "SE_INVALID_MIDI";       break;
        case SE_INVALID_NOTE:       return "SE_INVALID_NOTE";       break;
        case SE_INVALID_TIME:       return "SE_INVALID_TIME";       break;
        case SE_INVALID_GAP:        return "SE_INVALID_GAP";        break;
        case SE_INVALID_OPTION:     return "SE_INVALID_OPTION";     break;
        case SE_INVALID_REPEATS:    return "SE_INVALID_REPEATS";    break;
        case SE_FILE_READ_ERROR:    return "SE_FILE_READ_ERROR";                    break;
        case SE_INPUT_FILE_OPEN_ERROR:      return "SE_INPUT_FILE_OPEN_ERROR";      break;
        case SE_OUTPUT_FILE_OPEN_ERROR:     return "SE_OUTPUT_FILE_OPEN_ERROR";     break;
        case SE_FILE_ALREADY_OPEN_ERROR:    return "SE_FILE_ALREADY_OPEN_ERROR";    break;
        case SE_FILE_WRITE_ERROR:           return "SE_FILE_WRITE_ERROR";           break;
        case SE_INVALID_FILE_FORMAT:        return "SE_INVALID_FILE_FORMAT";        break;
        default:                            return "SE_UNKNOWN";                    break;
    }
}
