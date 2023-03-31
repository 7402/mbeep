## mbeep

---

### Description

This command-line tool plays a sequence of sine-wave tones on the default audio output device or writes a
sequence of sine-wave tones to a .wav file. Runs on Mac or Linux. Tones can be specified by frequency and
duration, MIDI notes, or text to be translated to Morse code.

### Usage

To produce a series of tones by explicitly specifying the frequency and duration of each tone, use the -f and -t
options. Separate each tone description with -p (play). The -g option specifies the gap to be used between each
tone. The -r option specifies how many times the tone is to be repeated.

To produce a series of tones corresponding to musical notes, use the -m option. To specify the tempo in quarter
notes per minute, use the -b option. For information about the format of the string describing these notes, type:
**mbeep --midi-help**

To produce a series of tones corresponding to Morse code, use the -c option. To specify the speed in words per
minute, use the -w option. For information about the format of the string describing these notes, type:
**mbeep --morse-help**

To read the MIDI string or Morse code text from a file, instead by specifying on the command line, use the -i option.

To write the audio data to disk file in .wav format, instead of sending it to the speaker, use the -o option.

For more information, see man page.

### Build and install

* macOS

```
cd path_to_directory
make
./mbeep --man-page > mbeep.1
sudo make install
```

* CentOS 7

```
sudo yum install openal-soft-devel
cd path_to_directory
make
./mbeep --man-page > mbeep.1
sudo make install
```

* Ubuntu 16.04

```
sudo apt-get install libopenal-dev
cd path_to_directory
make
./mbeep --man-page > mbeep.1
sudo make install
```

* Raspbian Bullseye

```
sudo apt-get install libopenal-dev
cd path_to_directory
make
./mbeep --man-page > mbeep.1
sudo make install
```

* Raspbian Bullseye with GPIO patch

```
cd path_to_directory
# assumes piezo speaker attached to GPIO pin BCM19
make GPIO=19
./mbeep --man-page > mbeep.1
sudo make install
```

### Notes

The common Unix beep tool ([https://github.com/johnath/beep/](https://github.com/johnath/beep/)) does not work on
the Mac, because the Mac does not have a simple piezo speaker on the motherboard. The mbeep tool uses the
OpenAL framework, which is built into the macOS system; mbeep can also be used on Unix systems that have
OpenAL installed.

The beep tool is good for reliable low-level output to the motherboard speaker on Unix systems, and it does not
depend on a sound card or higher-level sound options or libraries. The mbeep tool is good for sending sound to
headphones and speakers on Mac and Unix systems.

An experimental patch is available to send a square wave to a Raspberry Pi GPIO pin, which can be used with
a piezo element to generate a tone when no other audio device is available. Sound quality is low. (When the patch
is enabled, OpenAL is not used.)

See also [Computer Tools for Morse Code Practice](https://7402.org/blog/2018/computer-tools-for-morse-code-practice.html).

### License

BSD
