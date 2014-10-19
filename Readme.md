# Overview

This is an exploratory project for experimenting with LD3320 Automatic Speech Recognition (ASR) chip using teensy 3. LD3320 is Chinese made voice recognition chip that also supports MP3 playback. It is cheap and easy to use, but only has native support of Chinese through Han Yu Pin Yin.

# Wiring

The project connects teensy3 with a generic SD card module in addition to LD3320. LD3320 connects to the primary SPI pins of teensy, while SD card module connects to the alternative SPI pins. The reasons are LD3320 will not float SDO pin even when not selected. 

Pin connections are defined in main.cpp as follows,
```
#   define LED_PIN 0
#   define IR_PIN 5
#   define MD_PIN 6
#   define SPIS_PIN 4
#   define RSTB_PIN 3
#   define SCS_PIN ASR_SCS
#   define ASR_SDI 11
#   define ASR_SDO 12
#   define ASR_SCK 13
#   define ASR_SCS 10
#   define SD_SDI 7
#   define SD_SDO 8
#   define SD_SCK 14
#   define SD_SCS 2
```

# Bitlash Commands

This project uses [bitlash](http://bitlash.net) to add a shell enrionment for easy experimenting. In addition to the newly added commands, running script on SD card is also supported. Combining with the asr commands and infra red remote control commands, you can easily build new voice recongition enabled automation application by simply writing scripts on SD card. There are also scripts in the ``scripts`` directory that enables remote shell access through UART. So, simply add an OpenWRT WiFi router with USB support and you'll have a wirelessly reconfigurable ASR automation device.

Note, that all filename below must confirm to Microsoft DOS 8.3 short filename specification due to the limitation of Arduino SdFat library.

```bash

# Play mp3 file on sd card. If no filename is given, stop playing. Add a second argument of one
# to wait mp3 ending before exit. 
sd([<filename>][,1])
# Although LD3320 has a mechanism to determin the MP3 duration,
# I can't get it to work properly. It will wait about 2 seconds more before signalling the end
# in most cases. So instead, the function will read the MP3 header to determine the duration.
#
# MP3 file directory setting
sd(<0,1>,<directory>)
# When the first argument is 0, it sets the current MP3 directory to look for when start playing
# an MP3 file with relative path.
# When the first argument is 1, it sets the default MP3 directory which will be searched if the
# file is not found in the current MP3 directory.
# These function is here to make it easy to switch the whole set of voice prompt without requiring
# to change ASR menu, e.g. swith from male prompt to female.
#
# File manipulation functions
#
# Change the current working directory
cd(<directory name>)
#
# List the current directory content
dir
#
#  Delete the file
rm(<filename>)
#
#  Check if the file exists
exists(<filename>)
#
# Append text to a file. Create the file if not exists
append(<filename>,<text>)
#
# Append binary content to a file. Create the file if not exists.
append64(<filname>,<base 64 encoded string>)
#
# Make directory
md(<diretory name>)


# ASR commands
#
# ASR initialization
asr
#
# ASR global action settings
asr(0,<index>,<filename>[,<directory>])
# The supported index values are 
#   0: current menu. When an ASR round is finished, this script will be ran to setup the menu fo
#      the next round of ASR recognition.
#   1: default action will be executed if an ASR result is detected but no 
#      associated actions, 
#   2: error action will be executed if any chip configuration error has happed.
#   3: confirmation action will be ran if an ASR result is detected which requires a confirmation.
#      Unlike the current menu setting above, the confirmation menu item does not support action.
#      Specifiy an '!' in the action setting to confirm.
#      The default confirmation start a ASR loop waiting for the following keywords,
#      - mei cuo (means no error)
#      - hao wa (alright)
#      - bu shi la (no no no)
#      - bu shi (no)
#      - a (ah...)
#
# Start ASR recognition
asr(1)
#
# Add an ASR menu item
asr(2,<command in Chinese HanYuPinYin> [, <action script>])
# If no action script is given, if this menu item is recognized, the default action will be executed
# if there is one. 
#
# Add an ASR menu item that requires confirmation
asr(2,<command in Chinese HanYuPinYin>, <action script> [, <voice prompt>])
# <voice prompt> is an MP3 filename. If omitted, then the voice prompt shall be the same name as 
# <action script> appended with extension ".mp3".


# Infra Red signal transmitting
#
# General form
ir(<cmd>,...)
# If <cmd> is omitted, it will transmits the last received IR code.
#
# IR transmitting
ir(1,[code_type] [,<code_length>, <code_value>])
# If [code_type] is anything other than -1 (i.e. raw data), you need to specify the code length and
# value. Refer to arduino IRRemote library for code type. or just copy the output of ir(0). 
# For raw data sending, you can prepare the data using  the following raw buffer setup commands.
#
# IR raw buffer setup
ir(2,[value]...)
ir(3,[value]...)
# ir(2,...) resets the internal raw buffer index to 0, and squentially fill the buffer with
# the input argument. ir(3,...) continues the filling. This function exists because of the 
# limited line buffer size of bitlash, which is in turn limited by the arduino device memory.
```

See my [other project](https://github.com/realthunder/autotemp) for IR receiving command for duplicating any IR remote control code.

A complete ASR menu script set is available in directory ``scripts/bitlash/menu``. The firmware will automatically load the menu script named as ``/menu/main`` on SD card when powered on.

# Compile

To compile the project, you'll need to follow the teensy website [instruction](http://www.pjrc.com/teensy/first_use.html) to install the teensyduino. I'm a heavy vim user, so I also borrowed makefile from [here](http://forum.pjrc.com/threads/23605-Teensy-mk-port-of-Arduino-mk-Makefile). I slightly modified the makefile to make it work with ``cygwin``, and is available [here](https://gist.github.com/realthunder/9374708). You need to modify it to set ``ARDUINO_DIR`` to your arduino installation direction. You probably need to install the arduino in a none spaced directory to make cygwin happy.

You will need [bitlash](http://bitlash.net) to compile the project. You need a patched version from [here](https://github.com/realthunder/bitlash).

In order to achieve dynamic switching of SPI pins, you need to patch teensyduino's core library using ``patches/teensyduino-spi-dynamic-switch.patch``.

You'll also need a newer [SdFat](https://github.com/greiman/SdFat-beta) library than the one in Teensyduino. Make it use arduino's SPI library by defining ``USE_ARDUINO_SPI_LIBRARY`` as one in ``SdFat/SdFatConfig.h`` file. The project also uses [Base64](https://github.com/adamvr/arduino-base64.git) library to achieve binary file read from and write to SD card. Just clone the two libraries into the user library directory specified in the teensy makefile.

To compile and upload, simply use the following command
```
make upload
```
