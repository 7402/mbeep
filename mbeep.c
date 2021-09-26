//
// mbeep.c
// mbeep
//
// Copyright (C) 2018-2021 Michael Budiansky. All rights reserved.
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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define __USE_POSIX199309
#define __USE_XOPEN2K
#include <time.h>

#include "patterns.h"
#include "sound.h"
#include "text.h"

#define LINE_SIZE 1024
#define DEFAULT_WPM 20.0

// only available for macOS >= 10.12
#define USE_CLOCK_MONOTONIC 0

#if !USE_CLOCK_MONOTONIC
#include <sys/time.h>
#endif

int main(int argc, const char * argv[]) {
    SoundError error = SE_NO_ERROR;

    bool needs_init = true;
    double freq = DEFAULT;
    double msec = 200.0;
    int repeats = 1;
    double gap = 50.0;
    double bpm = 120.0;
    bool paris_standard = true;
    double dit = 1200.0 / DEFAULT_WPM;  // 20 wpm PARIS standard
    double word_speed = DEFAULT_WPM;
    double word_space_speed = DEFAULT_WPM;
    bool using_word_space_speed = false;
    double char_speed = DEFAULT;    // Farnsworth speed
    bool do_final_play = true;
    bool echo = false;
    bool print_fcc_wpm = false;

    FILE *in_file = NULL;
    FILE *out_file = NULL;
    char line[LINE_SIZE];

    for (int index = 1; index < argc && error == SE_NO_ERROR; index++) {
        //  -f  frequency
        if (strcmp(argv[index], "-f") == 0 && index + 1 < argc) {
            freq = atof(argv[++index]);
            if (freq < 20.0 || freq > 20000.0) error = SE_INVALID_FREQUENCY;

        //  -t  time in msec
        } else if (strcmp(argv[index], "-t") == 0 && index + 1 < argc) {
            msec = atof(argv[++index]);
            if (msec < 0.0) error = SE_INVALID_TIME;

        //  -g  gap in msec
        } else if (strcmp(argv[index], "-g") == 0 && index + 1 < argc) {
            gap = atof(argv[++index]);
            if (gap < 0.0) error = SE_INVALID_GAP;

        //  -r  repeat
        } else if (strcmp(argv[index], "-r") == 0 && index + 1 < argc) {
            repeats = atoi(argv[++index]);
            if (repeats < 0) error = SE_INVALID_REPEATS;

        //  -p  (play)
        } else if (strcmp(argv[index], "-p") == 0) {
            if (needs_init) {
                error = init_sound();
                needs_init = false;
            }

            if (error == SE_NO_ERROR) error = play(freq, msec, gap, repeats, out_file);

        //  -b  beats (quarter notes) per minute
        } else if (strcmp(argv[index], "-b") == 0) {
            bpm = atof(argv[++index]);
            if (bpm < 20.0 || bpm > 500.0) error = SE_INVALID_BPM;

        //  -m  string to send as midi notes
        } else if (strcmp(argv[index], "-m") == 0 && index + 1 < argc) {
            const char *str = argv[++index];

            if (needs_init) {
                error = init_sound();
                needs_init = false;
            }

            if (error == SE_NO_ERROR) error = play_midi(bpm, gap, str, out_file);

            if (error == SE_NO_ERROR) error = play_buffers();
            if (error == SE_NO_ERROR) error = wait_for_buffers();
            do_final_play = false;

        //  -m  (send input file as midi notes)
        } else if (strcmp(argv[index], "-m") == 0 && in_file != NULL) {
            if (needs_init) {
                error = init_sound();
                needs_init = false;
            }

            while (error == SE_NO_ERROR && fgets(line, LINE_SIZE, in_file) != NULL) {
                error = play_midi(bpm, gap, line, out_file);

                if (error == SE_NO_ERROR) error = play_buffers();
                if (error == SE_NO_ERROR) error = wait_for_buffers();

                if (echo) {
                    printf("%s", line);
                    fflush(stdout);
                }
            }

            if (error == SE_NO_ERROR && ferror(in_file) != 0) {
                error = SE_FILE_READ_ERROR;
            }

            if (in_file != stdin) fclose(in_file);
            in_file = NULL;
            do_final_play = false;

        //  -w  --paris-wpm words per minute, PARIS standard
        } else if (((strcmp(argv[index], "--paris-wpm") == 0) ||
                    (strcmp(argv[index], "-w") == 0)) && index + 1 < argc) {
            word_speed = atof(argv[++index]);
            if (!using_word_space_speed) word_space_speed = word_speed;
            paris_standard = true;
            dit = 60.0 * 1000.0 / (50.0 * word_speed);
            if (word_speed < 5.0 || word_speed > 60.0) error = SE_INVALID_WPM;

        //  --codex-wpm  words per minute, CODEX standard
        } else if (strcmp(argv[index], "--codex-wpm") == 0 && index + 1 < argc) {
            word_speed = atof(argv[++index]);
            if (!using_word_space_speed) word_space_speed = word_speed;
            paris_standard = false;
            dit = 60.0 * 1000.0 / (60.0 * word_speed);
            if (word_speed < 5.0 || word_speed > 60.0) error = SE_INVALID_WPM;

        //  -x  --farnsworth character speed
        } else if (((strcmp(argv[index], "--farnsworth") == 0) ||
                    (strcmp(argv[index], "-x") == 0)) && index + 1 < argc) {
            char_speed = atof(argv[++index]);
            if (char_speed < 5.0 || char_speed > 60.0) error = SE_INVALID_WPM;

        //  --wss  --word-space-speed word-space speed
        } else if ((strcmp(argv[index], "--wss") == 0) && index + 1 < argc) {
            word_space_speed = atof(argv[++index]);
            using_word_space_speed = true;
            if (word_space_speed < 5.0 || word_space_speed > 60.0) error = SE_INVALID_WPM;

        //  --fcc  print FCC wpm
        } else if (strcmp(argv[index], "--fcc") == 0) {
            print_fcc_wpm = true;

        //  -c  string to send as Morse code
        } else if (strcmp(argv[index], "-c") == 0 && index + 1 < argc) {
            if (needs_init) {
                error = init_sound();
                needs_init = false;
            }

            double farnsworth_ratio = char_speed == DEFAULT ? 1.0 : word_speed / char_speed;
            if (farnsworth_ratio > 1.0) error = SE_INVALID_WPM;

            double extra_word_gap = 0.0;
            if (word_space_speed < word_speed) {
                extra_word_gap = ((paris_standard ? 50.0 : 60.0) + 7.0) *
                dit * (word_speed / word_space_speed - 1.0);
            }

            int fcc_char_count = 0;

#if USE_CLOCK_MONOTONIC
            struct timespec ts;
            struct timespec te;

            clock_gettime(CLOCK_MONOTONIC, &ts);

#else
            struct timeval ts;
            struct timeval te;
            gettimeofday(&ts, NULL);
#endif

            if (error == SE_NO_ERROR) {
                const char *text = argv[++index];
                error = play_code(freq, dit, paris_standard, farnsworth_ratio,
                                  extra_word_gap, &fcc_char_count, text, out_file);
            }

            if (error == SE_NO_ERROR) error = play_buffers();
            if (error == SE_NO_ERROR) error = wait_for_buffers();

            do_final_play = false;

#if USE_CLOCK_MONOTONIC
            clock_gettime(CLOCK_MONOTONIC, &te);
            double elapsed = (double)(te.tv_sec - ts.tv_sec) + 1e-9 * (te.tv_nsec - ts.tv_nsec);

#else
            gettimeofday(&te, NULL);
            double elapsed = (double)(te.tv_sec - ts.tv_sec) + 1e-6 * (te.tv_usec - ts.tv_usec);
#endif

            if (print_fcc_wpm) {
                fprintf(stderr, "Elapsed %.1f seconds\nFCC char count %d\nFCC wpm %.1f\n", elapsed, fcc_char_count,
                        (fcc_char_count / 5.0) / (elapsed / 60.0));
            }

        //  -c  (send input file as Morse code)
        } else if (strcmp(argv[index], "-c") == 0 && in_file != NULL) {
            if (needs_init) {
                error = init_sound();
                needs_init = false;
            }

            double farnsworth_ratio = char_speed == DEFAULT ? 1.0 : word_speed / char_speed;
            if (farnsworth_ratio > 1.0) error = SE_INVALID_OPTION;

            double extra_word_gap = 0.0;
            if (word_space_speed < word_speed) {
                extra_word_gap = ((paris_standard ? 50.0 : 60.0) + 7.0) *
                    dit * (word_speed / word_space_speed - 1.0);
            }

            int fcc_char_count = 0;

#if USE_CLOCK_MONOTONIC
            struct timespec ts;
            struct timespec te;

            clock_gettime(CLOCK_MONOTONIC, &ts);

#else
            struct timeval ts;
            struct timeval te;
            gettimeofday(&ts, NULL);
#endif

            while (error == SE_NO_ERROR && fgets(line, LINE_SIZE, in_file) != NULL) {
                error = play_code(freq, dit, paris_standard, farnsworth_ratio,
                                  extra_word_gap, &fcc_char_count, line, out_file);

                if (error == SE_NO_ERROR) error = play_buffers();
                if (error == SE_NO_ERROR) error = wait_for_buffers();

                if (echo) {
                    printf("%s", line);
                    fflush(stdout);
                }
            }

#if USE_CLOCK_MONOTONIC
            clock_gettime(CLOCK_MONOTONIC, &te);
            double elapsed = (double)(te.tv_sec - ts.tv_sec) + 1e-9 * (te.tv_nsec - ts.tv_nsec);

#else
            gettimeofday(&te, NULL);
            double elapsed = (double)(te.tv_sec - ts.tv_sec) + 1e-6 * (te.tv_usec - ts.tv_usec);
#endif

            if (print_fcc_wpm) {
                fprintf(stderr, "Elapsed %.1f seconds\nFCC char count %d\nFCC wpm %.1f\n", elapsed, fcc_char_count,
                       (fcc_char_count / 5.0) / (elapsed / 60.0));
            }

            if (error == SE_NO_ERROR && ferror(in_file) != 0) {
                error = SE_FILE_READ_ERROR;
            }

            if (in_file != stdin) fclose(in_file);
            in_file = NULL;
            do_final_play = false;

        //  -i  input file for midi or code string (/dev/stdin for standard input)
        } else if (strcmp(argv[index], "-i") == 0 && index + 1 < argc) {
            if (in_file != NULL) {
                error = SE_FILE_ALREADY_OPEN_ERROR;

            } else {
                in_file = fopen(argv[++index], "r");
            }

            if (in_file == NULL) {
                error = SE_INPUT_FILE_OPEN_ERROR;
            }

        //  --play  play .wav file (for testing files written by mbeep)
        } else if (strcmp(argv[index], "--play") == 0 && index + 1 < argc) {
            if (needs_init) {
                error = init_sound();
                needs_init = false;
            }

            if (error == SE_NO_ERROR) {
                do_final_play = false;
                error = play_wav(argv[++index]);
            }
            
            if (error == SE_NO_ERROR) error = wait_for_buffers();

        //  -I  use stdin for midi or code string (same as -i /dev/stdin)
        } else if (strcmp(argv[index], "-I") == 0) {
            if (in_file != NULL) {
                error = SE_FILE_ALREADY_OPEN_ERROR;

            } else {
                in_file = stdin;
            }

            if (in_file == NULL) {
                error = SE_INPUT_FILE_OPEN_ERROR;
            }

        //  -e  (echo)
        } else if (strcmp(argv[index], "-e") == 0) {
            echo = true;

        //  -o --wav  output file for .wav
        } else if (((strcmp(argv[index], "-o") == 0) ||
                    (strcmp(argv[index], "--wav") == 0)) && index + 1 < argc && out_file == NULL) {
            
            out_file = fopen(argv[++index], "w");

            if (out_file == NULL) {
                error = SE_OUTPUT_FILE_OPEN_ERROR;
            }

            if (error == SE_NO_ERROR) {
                error = begin_wave_file(out_file);
            }

        //  --midi-help     print format for midi string
        } else if (strcmp(argv[index], "--midi-help") == 0) {
            midi_help();
            error = SE_EXIT;

        //  --morse-help    print information about Morse code
        } else if (strcmp(argv[index], "--morse-help") == 0) {
            morse_help();
            error = SE_EXIT;

        //  -v --version    print version of mbeep
        } else if ((strcmp(argv[index], "--version") == 0) ||
                   (strcmp(argv[index], "-v") == 0)) {
            version();
            error = SE_EXIT;

        //  -h --help       print help for mbeep
        } else if ((strcmp(argv[index], "--help") == 0) ||
                   (strcmp(argv[index], "-h") == 0)) {
            usage();
            error = SE_EXIT;

        //  --man-page      print source for man page
        } else if (strcmp(argv[index], "--man-page") == 0) {
            man_page_source();
            error = SE_EXIT;

        //  --licence       print copyright and license
        } else if (strcmp(argv[index], "--license") == 0) {
            license();
            error = SE_EXIT;

        } else {
            error = SE_INVALID_OPTION;
        }
    }

    if (error == SE_NO_ERROR && do_final_play) {
        if (needs_init) {
            init_sound();
            needs_init = false;
        }

        error = play(freq, msec, gap, repeats, out_file);

        if (error == SE_NO_ERROR) error = play_buffers();
        if (error == SE_NO_ERROR) error = wait_for_buffers();
    }

    if (in_file != NULL) {
        if (in_file != stdin) fclose(in_file);
        in_file = NULL;
    }

    if (out_file != NULL) {
        error = finish_wave_file(out_file);
        fclose(out_file);
        out_file = NULL;
    }

    switch (error) {
        case SE_NO_ERROR:                                                       break;
        case SE_EXIT:                                                           break;
        case SE_NO_DEVICE:          printf("Error: SE_NO_DEVICE\n");            break;
        case SE_NO_CONTEXT:         printf("Error: SE_NO_CONTEXT\n");           break;
        case SE_INVALID_NAME:       printf("Error: SE_INVALID_NAME\n");         break;
        case SE_INVALID_ENUM:       printf("Error: SE_INVALID_ENUM\n");         break;
        case SE_INVALID_VALUE:      printf("Error: SE_INVALID_VALUE\n");        break;
        case SE_INVALID_OPERATION:  printf("Error: SE_INVALID_OPERATION\n");    break;
        case SE_OUT_OF_MEMORY:      printf("Error: SE_OUT_OF_MEMORY\n");        break;
        case SE_INVALID_FREQUENCY:  printf("Error: SE_INVALID_FREQUENCY\n");    break;
        case SE_INVALID_WPM:        printf("Error: SE_INVALID_WPM\n");          break;
        case SE_INVALID_BPM:        printf("Error: SE_INVALID_BPM\n");          break;
        case SE_INVALID_MIDI:       printf("Error: SE_INVALID_MIDI\n");         break;
        case SE_INVALID_NOTE:       printf("Error: SE_INVALID_NOTE\n");         break;
        case SE_INVALID_TIME:       printf("Error: SE_INVALID_TIME\n");         break;
        case SE_INVALID_GAP:        printf("Error: SE_INVALID_GAP\n");          break;
        case SE_INVALID_OPTION:     printf("Error: SE_INVALID_OPTION\n");       break;
        case SE_INVALID_REPEATS:    printf("Error: SE_INVALID_REPEATS\n");      break;
        case SE_FILE_READ_ERROR:            printf("Error: SE_FILE_READ_ERROR\n");          break;
        case SE_INPUT_FILE_OPEN_ERROR:      printf("Error: SE_INPUT_FILE_OPEN_ERROR\n");    break;
        case SE_OUTPUT_FILE_OPEN_ERROR:     printf("Error: SE_OUTPUT_FILE_OPEN_ERROR\n");   break;
        case SE_FILE_ALREADY_OPEN_ERROR:    printf("Error: SE_FILE_ALREADY_OPEN_ERROR\n");  break;
        case SE_FILE_WRITE_ERROR:           printf("Error: SE_FILE_WRITE_ERROR\n");         break;

        case SE_UNKNOWN:
        default:
            printf("Error: unknown\n");
            break;
    };

    close_sound();

    return 0;
}

