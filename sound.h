//
// sound.h
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

#ifndef sound_h
#define sound_h

#define SILENCE 0.0

typedef enum SoundError {
    SE_NO_ERROR = 0,
    SE_EXIT,
    SE_NO_DEVICE,
    SE_NO_CONTEXT,
    SE_UNKNOWN,
    SE_INVALID_NAME,
    SE_INVALID_ENUM,
    SE_INVALID_VALUE,
    SE_INVALID_OPERATION,
    SE_OUT_OF_MEMORY,
    SE_INVALID_FREQUENCY,
    SE_INVALID_WPM,
    SE_INVALID_BPM,
    SE_INVALID_MIDI,
    SE_INVALID_NOTE,
    SE_INVALID_TIME,
    SE_INVALID_GAP,
    SE_INVALID_OPTION,
    SE_INVALID_REPEATS,
    SE_FILE_READ_ERROR,
    SE_INPUT_FILE_OPEN_ERROR,
    SE_OUTPUT_FILE_OPEN_ERROR,
    SE_FILE_ALREADY_OPEN_ERROR,
    SE_FILE_WRITE_ERROR
} SoundError;

SoundError init_sound(void);
SoundError fill_buffer_or_file(double freq, double msec, FILE *file);
SoundError fill_file(double freq, double msec, FILE *file);
SoundError fill_buffer(double freq, double msec);

SoundError play_buffers(void);
SoundError wait_for_buffers(void);
void close_sound(void);

SoundError begin_wave_file(FILE *file);
SoundError finish_wave_file(FILE *file);

#endif /* sound_hpp */


