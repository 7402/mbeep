//
// text.c
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
#include <stdio.h>
#include <string.h>

#include "text.h"

void version(void)
{
    printf("mbeep 0.9.2\n");
}

void usage(void)
{
    printf("Tool for sending tone or sequence of tones to audio output or .wav file.\n"
           "\n"
           "Usage:\n"
           "  mbeep [-o <output_file] [ [-f <frequency>] [-t <time_in_msec>]\n"
           "                            [-g <gap_in_msec>] [-r <repeats>] [-p] ]...\n"
           "  mbeep [-o <output_file] [-b <beats_per_min>] -m <midi_string>\n"
           "  mbeep [-o <output_file] [-b <beats_per_min>] -i <input_file> -m\n"
           "  mbeep [-o <output_file] [-w <words_per_min>] -c <string_to_send_as_morse_code>\n"
           "  mbeep [-o <output_file] [-w <words_per_min>] -i <input_file>  -c\n"
           "  mbeep -h | --help\n"
           "  mbeep -v | --version\n"
           "  mbeep --midi-help\n"
           "  mbeep --morse-help\n"
           "  mbeep --license\n"
           "  mbeep --man-page\n"
           "\n"
           "Options:\n"
           "  -f <freq>         Frequency of tone in Hz [default: 750 for code, else 440]\n"
           "  -t <time>         Duration of tone in msec [default: 200]\n"
           "  -g <gap>          Gap between tones in msec [default: 50]\n"
           "  -r <repeats>      Number of times to repeat tone [default: 1]\n"
           "  -p                Play tone. Used when specifying sequence of multiple tones.\n"
           "  -o <output>       Write .wav file containing tones\n"
           "  -b <tempo>        Quarter notes per minute [default: 120]\n"
           "  -w <wpm>          Morse code speed in PARIS words per minute [default: 20]\n"
           "  --codex-wpm <wpm> Morse code speed in CODEX words per minute [default: 16 2/3]\n"
           "  -x <speed>        Character speed for Farnsworth Morse code timing\n"
           "  -i <input>        Input file or path for -m or -c options\n"
           "  -m <string>       Send sequence of MIDI notes specified by string\n"
           "  -m                Send sequence of MIDI notes specified by input file\n"
           "  -c <string>       Send text in string as Morse code\n"
           "  -c                Send text in input file as Morse code\n"
           "\n"
           "  -h --help     Show this screen.\n"
           "  --version     Show version.\n"
           "  --midi-help   Show format of MIDI string\n"
           "  --morse-help  Show Morse code\n"
           "  --license     Show software copyright and license\n"
           "  --man_page    Show source for man page\n");
}

void midi_help(void)
{
    printf("A simple MIDI string looks like this:\n"
           "   60q 64q 67h\n"
           "or like this:\n"
           "   C4q E4q G4h\n"
           "  \n"
           "A MIDI string consists of one or more MIDI notes. Notes are separated by a space\n"
           "or carriage return. Each note consists of a pitch designator (such as C5 for\n"
           "middle-C) followed by a duration designator (such as q for quarter note).\n"
           "\n"
           "A pitch designator is one of the following:\n"
           "a) a MIDI number (in the range 16-127)\n"
           "b) or, a letter (A-G) followed by optional sharp or flat (# or b) followed by an \n"
           "      octave number (0-9)\n"
           "c) or, the letter R, representing a rest.\n"
           "\n"
           "A simple duration designator consists of one of the following letters\n"
           "   d  double whole note\n"
           "   w  whole note\n"
           "   h  half note\n"
           "   q  quarter note\n"
           "   e  eighth note\n"
           "   s  sixteenth note\n"
           "   t  thirty-second note\n"
           "The letter can be followed by a period to indicate a dotted note or by a 3 to\n"
           "indicate a triplet. Other durations can indicated by combining a series of\n"
           "simple durations. For example, h.e is a duration equal to a dotted half note\n"
           "plus an eighth note.\n\n");

    // print table of MIDI notes
    const int rows = 38;
    for (int k = 16; k <= 16 + rows - 1; k++) {
        for (int n = k; n <= k + 2 * rows; n+= rows) {
            int octave = n / 12 - 1;
            char note[3];
            double freq = pow(2.0, (n - 69.0) / 12.0) * 440.0;

            switch (n % 12) {
                case 0:     strcpy(note, " C");     break;
                case 1:     strcpy(note, "C#");     break;
                case 2:     strcpy(note, " D");     break;
                case 3:     strcpy(note, "D#");     break;
                case 4:     strcpy(note, " E");     break;
                case 5:     strcpy(note, " F");     break;
                case 6:     strcpy(note, "F#");     break;
                case 7:     strcpy(note, " G");     break;
                case 8:     strcpy(note, "G#");     break;
                case 9:     strcpy(note, " A");     break;
                case 10:    strcpy(note, "A#");     break;
                case 11:    strcpy(note, " B");     break;
            }

            if (n > k) printf("         ");

            if (n <= 127) printf("%3d  %s%1d  %8.2f", n, note, octave, freq);
        }
        printf("\n");
    }

    printf("\n");
}

void morse_help(void)
{

    printf("Morse Code\n"
           "\n"
           "Letters\n"
           "A  .-          J  .---        S  ...\n"
           "B  -...        K  -.-         T  -\n"
           "C  -.-.        L  .-..        U  ..-\n"
           "D  -..         M  --          V  ...-\n"
           "E  .           N  -.          W  .--\n"
           "F  ..-.        O  ---         X  -..-\n"
           "G  --.         P  .--.        Y  -.--\n"
           "H  ....        Q  --.-        Z  --..\n"
           "I  ..          R  .-.        \n"
           "\n"
           "Accented E (ITU definition)\n"
           "É  ..-..\n"
           "\n"
           "Digits\n"
           "1  .----       6  -....\n"
           "2  ..---       7  --...\n"
           "3  ...--       8  ---..\n"
           "4  ....-       9  ----.\n"
           "5  .....       0  -----\n"
           "\n"
           "Common punctuation and prosigns (FCC code tests)\n"
           ".  .-.-.-\n"
           ",  --..--\n"
           "?  ..--..\n"
           "/  -..-.\n"
           "+  .-.-.    <AR>\n"
           "=  -...-    <BT>\n"
           "*  ...-.-   <SK>  (* is unassigned; used by mbeep only)\n"
           "\n"
           "Other ITU punctuation\n"
           ":  ---...\n"
           "'  .----.\n"
           "-  -....-\n"
           "(  -.--.\n"
           ")  -.--.-\n"
           "\"  .-..-.\n"
           "@  .--.-.\n"
           "                \n"
           "Unofficial punctuation (non-ITU)\n"
           "$  ...-..-\n"
           ";  -.-.-.\n"
           "_  ..--.-\n"
           "!  -.-.--   <KW>\n"
           "&  .-...    <AS>\n"
           "\n"
           "Other prosigns, represented by unassigned symbols\n"
           "^  ...-.    <VE>\n"
           "#  -.-.-    <CT>\n"
           "|  .-.-     <AA>\n"
           "%%  -.--.    <KN>\n"
           "\n"
           "Note: * ^ # | %% are used by mbeep only - they are not\n"
           "recognized elsewhere as Morse code.\n");
}

void license(void)
{
    printf("Copyright (C) 2018 Michael Budiansky. All rights reserved.\n"
           "\n"
           "Redistribution and use in source and binary forms, with or without modification, are permitted\n"
           "provided that the following conditions are met:\n"
           "\n"
           "Redistributions of source code must retain the above copyright notice, this list of conditions\n"
           "and the following disclaimer.\n"
           "\n"
           "Redistributions in binary form must reproduce the above copyright notice, this list of conditions\n"
           "and the following disclaimer in the documentation and/or other materials provided with the\n"
           "distribution.\n"
           "\n"
           "THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS \"AS IS\" AND ANY EXPRESS OR\n"
           "IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND\n"
           "FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR\n"
           "CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL\n"
           "DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,\n"
           "DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,\n"
           "WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY\n"
           "WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.\n");
}

void man_page_source(void)
{
    printf(".TH mbeep 1\n"
           "\n"
           ".SH NAME\n"
           "mbeep \\- send tone or sequence of tones to audio output or .wav file\n"
           "\n"
           ".SH SYNOPSIS\n"
           ".nf\n"
           "\\fBmbeep\\fR [ [\\fB\\-o\\fR \\fIFILE\\fR] [\\fB\\-f\\fR \\fIFREQ\\fR] [\\fB\\-t\\fR \\fIMSEC\\fR] [\\fB\\-g\\fR \\\n"
           "\\fIMSEC\\fR] [\\fB\\-r\\fR \\fIREPEATS\\fR] [\\fB\\-p\\fR] ]...\n"
           "\\fBmbeep\\fR [\\fB\\-o\\fR \\fIFILE\\fR] [\\fB\\-b\\fR \\fIBPM\\fR] \\fB\\-m\\fR \\fITEXT\\fR\n"
           "\\fBmbeep\\fR [\\fB\\-o\\fR \\fIFILE\\fR] [\\fB\\-b\\fR \\fIBPM\\fR] \\fB\\-i\\fR \\fIFILE\\fR \\fB\\-m\\fR\n"
           "\\fBmbeep\\fR [\\fB\\-o\\fR \\fIFILE\\fR] [\\fB\\-w\\fR \\fIWPM\\fR] [\\fB\\-x\\fR \\fICHAR_SPEED\\fR] \\fB\\-c\\fR \\fITEXT\\fR\n"
           "\\fBmbeep\\fR [\\fB\\-o\\fR \\fIFILE\\fR] [\\fB\\-w\\fR \\fIWPM\\fR] [\\fB\\-x\\fR \\fICHAR_SPEED\\fR] \\fB\\-i\\fR \\fIFILE\\fR \\fB\\-c\\fR\n"
           "\\fBmbeep\\fR \\fB\\-h\\fR | \\fB\\-v\\fR | \\fB\\-\\-midi\\-help\\fR | \\fB\\-\\-morse\\-help\\fR | \\\n"
           "\\fB\\-\\-license\\fR | \\fB\\-\\-man\\-page\\fR\n"
           ".fi\n"
           "\n"
           ".SH DESCRIPTION\n"
           "Plays a sequence of sine\\-wave tones on the default audio output device or writes a sequence of\n"
           "sine\\-wave tones to a .wav file.\n"
           "Runs on Mac or Linux command line. Tones can be specified by frequency and duration, MIDI notes,\n"
           "or text to be translated to Morse code.\n"
           "\n"
           "To produce a series of tones by explicitly specifying the frequency and duration of each tone, use\n"
           "the \\-f and \\-t options. Separate each tone description with -p (play). The \\-g option specifies\n"
           "the gap to be used between each tone. The \\-r option specifies how many times the tone\n"
           "is to be repeated.\n"
           "\n"
           "To produce a series of tones corresponding to musical notes, use the \\-m option. To specify the\n"
           "tempo in quarter notes per minute, use the \\-b option. For information about the format of the\n"
           "string describing these notes, type:\n"
           "    \\fBmbeep \\-\\-midi\\-help\\fR\n"
           "\n"
           "To produce a series of tones corresponding to Morse code, use the \\-c option. To specify the\n"
           "speed in words per minute, use the \\-w option. For information about the format of the\n"
           "string describing these notes, type:\n"
           "    \\fBmbeep \\-\\-morse\\-help\\fR\n"
           "\n"
           "To read the MIDI string or Morse code text from a file, instead by specifying on the command line,\n"
           "use the \\-i option.\n"
           "\n"
           "To write the audio data to disk file in .wav format, instead of sending it to the speaker,\n"
           "use the \\-o option.\n"
           "\n"
           ".SH OPTIONS\n"
           "\n"
           ".TP\n"
           ".BR \\-f \" \" \\fIFREQ\\fR\n"
           "Frequency of tone in Hz. Default is 750 for code, else 440.\n"
           "\n"
           ".TP\n"
           ".BR \\-t \" \" \\fITIME\\fR\n"
           "Duration of tone in msec. Default is 200.\n"
           "\n"
           ".TP\n"
           ".BR \\-g \" \" \\fIGAP\\fR\n"
           "Gap between tones in msec. Default is 50.\n"
           "\n"
           ".TP\n"
           ".BR \\-r \" \" \\fIREPEATS\\fR\n"
           "Number of times to repeat tone. Default is 1.\n"
           "\n"
           ".TP\n"
           ".BR \\-p\n"
           "Play tone. Used when specifying sequence of multiple tones. Do not use at end of command line,\n"
           "unless you want tone to play twice.\n"
           "\n"
           ".TP\n"
           ".BR \\-o \" \" \\fIOUTPUT\\fR\n"
           "Write .wav file containing tones.\n"
           "\n"
           ".TP\n"
           ".BR \\-b \" \" \\fITEMPO\\fR\n"
           "Quarter notes per minute. Default is 120.\n"
           "\n"
           ".TP\n"
           ".BR \\-w \", \" \\-\\-paris\\-wpm \" \" \\fIWPM\\fR\n"
           "Morse code speed in words per minute (PARIS standard). Default is 20.\n"
           "\n"
           ".TP\n"
           ".BR \\-\\-codex\\-wpm \" \" \\fIWPM\\fR\n"
           "Morse code speed in words per minute (CODEX standard). Default is 16 2/3.\n"
           "\n"
           ".TP\n"
           ".BR \\-x \", \" \\-\\-farnsworth \" \" \\fICHAR_SPEED\\fR\n"
           "Character speed for Farnsworth Morse code timing. Default is same as words per minute.\n"
           "\n"
           ".TP\n"
           ".BR \\-i \" \" \\fIINPUT\\fR\n"
           "Input file or path for -m or -c options.\n"
           "\n"
           ".TP\n"
           ".BR \\-m \" \" \\fISTRING\\fR\n"
           "Send sequence of MIDI notes specified by string.\n"
           "\n"
           ".TP\n"
           ".BR \\-m\n"
           "Send sequence of MIDI notes specified by input file.\n"
           "\n"
           ".TP\n"
           ".BR \\-c \" \" \\fISTRING\\fR\n"
           "Send text in string as Morse code.\n"
           "\n"
           ".TP\n"
           ".BR \\-c\n"
           "Send text in input file as Morse code.\n"
           "\n"
           ".TP\n"
           ".BR \\-h \", \" \\-\\-help\\fR\n"
           "Show help message.\n"
           "\n"
           ".TP\n"
           ".BR \\-v \", \" \\-\\-version\n"
           "Show version.\n"
           "\n"
           ".TP\n"
           ".BR \\-\\-midi\\-help\n"
           "Show information about format of MIDI string.\n"
           "\n"
           ".TP\n"
           ".BR \\-\\-morse\\-help\n"
           "Show information about Morse code characters.\n"
           "\n"
           ".TP\n"
           ".BR \\-\\-license\n"
           "Show software copyright and license.\n"
           "\n"
           ".TP\n"
           ".BR \\-\\-man\\-page\n"
           "Show source for this man page\n"
           "\n"
           ".SH EXAMPLES\n"
           "Play a sequence of tones:\n"
           ".PP\n"
           ".nf\n"
           ".RS\n"
           "\\fBmbeep -t 600 -f 1174.7 -p -f 1318.5 -p -f 1046.5 -p -f 523.3 -p -t 900 -f 784.0\\fR\n"
           ".RE\n"
           ".fi\n"
           ".PP\n"
           "\n"
           "Play a melody:\n"
           ".PP\n"
           ".nf\n"
           ".RS\n"
           "\\fBmbeep -m \"C5q C5q C5e. D5s E5q E5e. D5s E5e. F5s G5h\"\\fR\n"
           ".RE\n"
           ".fi\n"
           ".PP\n"
           "\n"
           "Play Morse code:\n"
           ".PP\n"
           ".nf\n"
           ".RS\n"
           "\\fBmbeep -c \"W1AW DE K1JMQ +\"\\fR\n"
           ".RE\n"
           ".fi\n"
           ".PP\n"
           "\n"
           "Play Morse code from standard input:\n"
           ".PP\n"
           ".nf\n"
           ".RS\n"
           "\\fBmbeep -i /dev/stdin -c\\fR\n"
           ".RE\n"
           ".fi\n"
           ".PP\n"
           "\n"
           "Write melody to .wav disk file:\n"
           ".PP\n"
           ".nf\n"
           ".RS\n"
           "\\fBmbeep -o daisy.wav -b 180 -m \"d6h. b5h. g5h. d5h rq e5q f#5q g5q e5h g5q d5h.h\"\\fR\n"
           ".RE\n"
           ".fi\n"
           ".PP\n"
           "\n"
           ".SH SEE ALSO\n"
           ".BR beep (1)\n"
           "\n"
           ".SH NOTES\n"
           "The common beep tool on Unix systems does not work on the Mac, because the Mac does not have a simple piezo\n"
           "speaker on the motherboard. The mbeep tool uses the OpenAL framework, which is built into the macOS system;\n"
           "mbeep can also be used on Unix systems that have OpenAL installed.\n"
           "\n"
           "The beep tool is good for reliable low\\-level output to the motherboard speaker on Unix systems, and it does\n"
           "not depend on a sound card or higher\\-level sound options or libraries. The mbeep tool is good for sending sound to headphones\n"
           "and speakers on Mac and Unix systems.\n"
           "\n"
           "The PARIS standard for words per minute speed assumes each word is the same length in Morse code as the word PARIS. This is the most common standard for code speed and is appropriate for plain\\-language text.\n"
           "\n"
           "The CODEX standard for words per minute speed assumes each word is the same length in Morse code as the word CODEX. This standard is appropriate for calculating speed for five\\-letter code groups.\n"
           "\n"
           ".SH AUTHOR\n"
           "Michael Budiansky \\fIhttps://www.7402.org/email\\fR\n"
           "\n");
}

