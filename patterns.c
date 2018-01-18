//
//  patterns.c
//  mbeep
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

#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "patterns.h"

// play tone followed by gap
SoundError play(double freq, double msec, double gap, int repeats, FILE *out_file)
{
    for (int k = 0; k < repeats; k++) {
        fill_buffer_or_file(freq, msec, out_file);
        fill_buffer_or_file(SILENCE, gap, out_file);
    }

    return SE_NO_ERROR;
}

// play MIDI notes
SoundError play_midi(double bpm, double gap, const char *text, FILE *out_file)
{
    SoundError error = SE_NO_ERROR;
    char *str = (char *)malloc(strlen(text + 1));

    if (str == NULL) {
        error = SE_OUT_OF_MEMORY;

    } else {
        char *token;

        strcpy(str, text);
        token = strtok(str, " \t\r\n");

        // do for each token in string
        while (token != NULL && error == SE_NO_ERROR) {
            char *cp = token;
            double freq = 0.0;
            bool is_rest = false;
            double msec = 0.0;

            // rest
            if (*cp == 'r' || *cp == 'R') {
                is_rest = true;
                cp++;

            // MIDI number
            } else if (isdigit(*cp)) {
                int midi = atoi(token);
                if (midi < 16 || midi > 127) {
                    error = SE_INVALID_MIDI;

                } else {
                    freq = pow(2.0, (midi - 69.0) / 12.0) * 440.0;
                    while (isdigit(*cp)) cp++;
                }

            // named pitch
            } else if (toupper(*cp) >= 'A' && toupper(*cp) <= 'G') {
                int midi = 0;
                switch(toupper(*cp)) {
                    case 'C':   midi = 0;   break;
                    case 'D':   midi = 2;   break;
                    case 'E':   midi = 4;   break;
                    case 'F':   midi = 5;   break;
                    case 'G':   midi = 7;   break;
                    case 'A':   midi = 9;   break;
                    case 'B':   midi = 11;  break;
                }

                cp++;
                if (*cp == '#') {
                    midi++;
                    cp++;

                } else if (*cp == 'b') {
                    midi--;
                    cp++;
                }

                // octave number
                if (isdigit(*cp)) {
                    midi += 12 * (*cp - '0' + 1);
                    cp++;

                } else {
                    error = SE_INVALID_NOTE;
                }

                if (midi < 16 || midi > 127) {
                    error = SE_INVALID_MIDI;

                } else {
                    freq = pow(2.0, (midi - 69.0) / 12.0) * 440.0;
                }

            } else {
                error = SE_INVALID_NOTE;
            }

            // if no note given, assume quarter note
            if (*cp == '\0') {
                double quarters = 1.0;
                msec = 1000.0 * quarters * 60.0 / bpm - gap;

            } else {
                while (*cp != '\0' && error == SE_NO_ERROR) {
                    double quarters = 0.0;
                    switch (toupper(*cp)) {
                        case 'D':  quarters = 8;         break;
                        case 'W':  quarters = 4;         break;
                        case 'H':  quarters = 2;         break;
                        case 'Q':  quarters = 1;         break;
                        case 'E':  quarters = 0.5;       break;
                        case 'S':  quarters = 0.25;      break;
                        case 'T':  quarters = 0.125;     break;
                        default:
                            error = SE_INVALID_NOTE;
                            break;
                    }

                    cp++;

                    // dotted
                    if (error == SE_NO_ERROR && *cp == '.') {
                        quarters *= 1.5;
                        cp++;
                    }

                    // triplet
                    if (error == SE_NO_ERROR && *cp == '3') {
                        quarters *= 2.0 / 3.0;
                        cp++;
                    }

                    if (error == SE_NO_ERROR) {
                        msec += 1000.0 * quarters * 60.0 / bpm;
                    }
                }
            }

            if (msec <= gap) {
                msec += gap;
                gap = 0.0;
            }

            if (error == SE_NO_ERROR) {
                if (is_rest) {
                    error = fill_buffer_or_file(SILENCE, msec, out_file);

                } else {
                    error = fill_buffer_or_file(freq, msec - gap, out_file);
                    if (error == SE_NO_ERROR) fill_buffer_or_file(SILENCE, gap, out_file);
                }
            }

            token = strtok(NULL, " \t\r\n");
        }

        free(str);
        str = NULL;
    }

    return error;
}

SoundError play_code(double freq, double dit, const char *text, FILE *out_file)
{
    bool was_space = false;
    bool is_space = false;

    for (int k = 0; k < strlen(text); k++) {
        unsigned char c = (unsigned char)toupper(text[k]);
        char sequence[16];

        sequence[0] = '\0';

        is_space = false;
        switch (c) {
            // letters
            case 'A':   strcpy(sequence, ".-");     break;
            case 'B':   strcpy(sequence, "-...");   break;
            case 'C':   strcpy(sequence, "-.-.");   break;
            case 'D':   strcpy(sequence, "-..");    break;
            case 'E':   strcpy(sequence, ".");      break;
            case 'F':   strcpy(sequence, "..-.");   break;
            case 'G':   strcpy(sequence, "--.");    break;
            case 'H':   strcpy(sequence, "....");   break;
            case 'I':   strcpy(sequence, "..");     break;
            case 'J':   strcpy(sequence, ".---");   break;
            case 'K':   strcpy(sequence, "-.-");    break;
            case 'L':   strcpy(sequence, ".-..");   break;
            case 'M':   strcpy(sequence, "--");     break;
            case 'N':   strcpy(sequence, "-.");     break;
            case 'O':   strcpy(sequence, "---");    break;
            case 'P':   strcpy(sequence, ".--.");   break;
            case 'Q':   strcpy(sequence, "--.-");   break;
            case 'R':   strcpy(sequence, ".-.");    break;
            case 'S':   strcpy(sequence, "...");    break;
            case 'T':   strcpy(sequence, "-");      break;
            case 'U':   strcpy(sequence, "..-");    break;
            case 'V':   strcpy(sequence, "...-");   break;
            case 'W':   strcpy(sequence, ".--");    break;
            case 'X':   strcpy(sequence, "-..-");   break;
            case 'Y':   strcpy(sequence, "-.--");   break;
            case 'Z':   strcpy(sequence, "--..");   break;

            // UTF-8 for É or é
            case 0xC3:
                c = (unsigned char)text[++k];
                if (c == 0x89 || c == 0xA9) {
                    strcpy(sequence, "..-..");

                } else {
                    is_space = true;
                }
                break;

            // digits
            case '0':   strcpy(sequence, "-----");  break;
            case '1':   strcpy(sequence, ".----");  break;
            case '2':   strcpy(sequence, "..---");  break;
            case '3':   strcpy(sequence, "...--");  break;
            case '4':   strcpy(sequence, "....-");  break;
            case '5':   strcpy(sequence, ".....");  break;
            case '6':   strcpy(sequence, "-....");  break;
            case '7':   strcpy(sequence, "--...");  break;
            case '8':   strcpy(sequence, "---..");  break;
            case '9':   strcpy(sequence, "----.");  break;

            // common punctuation and prosigns (FCC code test)
            case '.':   strcpy(sequence, ".-.-.-");     break;
            case ',':   strcpy(sequence, "--..--");     break;
            case '?':   strcpy(sequence, "..--..");     break;
            case '/':   strcpy(sequence, "-..-.");      break;
            case '+':   strcpy(sequence, ".-.-.");      break;  // <AR>
            case '=':   strcpy(sequence, "-...-");      break;  // <BT>
            case '*':   strcpy(sequence, "...-.-");     break;  // <SK>

            // other ITU punctuation
            case ':':   strcpy(sequence, "---...");     break;
            case '\'':  strcpy(sequence, ".----.");     break;
            case '-':   strcpy(sequence, "-....-");     break;
            case '(':   strcpy(sequence, "-.--.");      break;
            case ')':   strcpy(sequence, "-.--.-");     break;
            case '\"':  strcpy(sequence, ".-..-.");     break;
            case '@':   strcpy(sequence, ".--.-.");     break;

            // unofficial punctuation
            case '$':   strcpy(sequence, "...-..-");    break;
            case ';':   strcpy(sequence, "-.-.-.");     break;
            case '_':   strcpy(sequence, "..--.-");     break;
            case '!':   strcpy(sequence, "-.-.--");     break;  // <KW>
            case '&':   strcpy(sequence, ".-...");      break;  // <AS>

            // other prosigns, represented by unused character assignments
            case '^':   strcpy(sequence, "...-.");      break;  // <VE>
            case '#':   strcpy(sequence, "-.-.-");      break;  // <CT>
            case '|':   strcpy(sequence, ".-.-");       break;  // <AA>
            case '%':   strcpy(sequence, "-.--.");      break;  // <KN>

            default:    is_space = true;            break;
        }

        for (int i = 0; i < strlen(sequence); i++) {
            if (sequence[i] == '.') {
                fill_buffer_or_file(freq, dit, out_file);
                fill_buffer_or_file(SILENCE, dit, out_file);

            } else {
                fill_buffer_or_file(freq, 3 * dit, out_file);
                fill_buffer_or_file(SILENCE, dit, out_file);
            }
        }

        if (strlen(sequence) > 0) fill_buffer_or_file(SILENCE, 2 * dit, out_file);

        if (is_space && !was_space) fill_buffer_or_file(SILENCE, 4 * dit, out_file);

        was_space = is_space;
    }

    return SE_NO_ERROR;
}

