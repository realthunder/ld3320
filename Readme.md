# Overview

Experiment with LD3320 Automatic Speech Recognition (ASR) chip using teensy 3. 

# Wiring

The project connects teensy3 with a generic SD card module in addition to LD3320. LD3320 connects to the primary SPI pins of teensy, while SD card module connects to the alternative SPI pins. The reasons are LD3320 will not float SDO pin even when not selected. In order to achieve dynamic switching of SPI pins, you need the following patch of teensyduino's core library.

```patch
--- a/hardware/teensy/cores/teensy3/avr_emulation.h	2014-10-14 17:33:50.996546900 +0800
+++ b/hardware/teensy/cores/teensy3/avr_emulation.h	2014-10-14 17:33:26.605921900 +0800
@@ -1008,18 +1008,24 @@
 	inline void enable_pins(void) __attribute__((always_inline)) {
 		//serial_print("enable_pins\n");
 		if ((pinout & 1) == 0) {
+            CORE_PIN7_CONFIG = PORT_PCR_SRE | PORT_PCR_DSE | PORT_PCR_MUX(1);
 			CORE_PIN11_CONFIG = PORT_PCR_DSE | PORT_PCR_MUX(2); // DOUT/MOSI = 11 (PTC6)
 		} else {
+            CORE_PIN11_CONFIG = PORT_PCR_SRE | PORT_PCR_DSE | PORT_PCR_MUX(1);
 			CORE_PIN7_CONFIG = PORT_PCR_MUX(2); // DOUT/MOSI = 7 (PTD2)
 		}
 		if ((pinout & 2) == 0) {
+            CORE_PIN8_CONFIG = PORT_PCR_SRE | PORT_PCR_DSE | PORT_PCR_MUX(1);
 			CORE_PIN12_CONFIG = PORT_PCR_MUX(2);  // DIN/MISO = 12 (PTC7)
 		} else {
+            CORE_PIN12_CONFIG = PORT_PCR_SRE | PORT_PCR_DSE | PORT_PCR_MUX(1);
 			CORE_PIN8_CONFIG = PORT_PCR_MUX(2);  // DIN/MISO = 8 (PTD3)
 		}
 		if ((pinout & 4) == 0) {
+            CORE_PIN14_CONFIG = PORT_PCR_SRE | PORT_PCR_DSE | PORT_PCR_MUX(1);
 			CORE_PIN13_CONFIG = PORT_PCR_DSE | PORT_PCR_MUX(2); // SCK = 13 (PTC5)
 		} else {
+            CORE_PIN13_CONFIG = PORT_PCR_SRE | PORT_PCR_DSE | PORT_PCR_MUX(1);
 			CORE_PIN14_CONFIG = PORT_PCR_MUX(2); // SCK = 14 (PTD1)
 		}
 	}
@@ -1028,6 +1034,9 @@
 		CORE_PIN11_CONFIG = PORT_PCR_SRE | PORT_PCR_DSE | PORT_PCR_MUX(1);
 		CORE_PIN12_CONFIG = PORT_PCR_SRE | PORT_PCR_DSE | PORT_PCR_MUX(1);
 		CORE_PIN13_CONFIG = PORT_PCR_SRE | PORT_PCR_DSE | PORT_PCR_MUX(1);
+        CORE_PIN7_CONFIG = PORT_PCR_SRE | PORT_PCR_DSE | PORT_PCR_MUX(1);
+        CORE_PIN8_CONFIG = PORT_PCR_SRE | PORT_PCR_DSE | PORT_PCR_MUX(1);
+        CORE_PIN14_CONFIG = PORT_PCR_SRE | PORT_PCR_DSE | PORT_PCR_MUX(1);
 	}
 };
 extern SPCRemulation SPCR;
```

You'll also need a newer [SdFat](https://github.com/greiman/SdFat-beta) library than the one in Teensyduino. Make it use arduino's SPI library by define `USE_ARDUINO_SPI_LIBRARY` as one in `SdFat/SdFatConfig.h` file.

Pin connections are defined in main.cpp as follows,
```
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

This project uses [bitlash](http://bitlash.net) to add a shell enrionment for easy experimenting. Newly added commands are,

```bash
# Toggle ASR function
asr


# Play mp3 file on sd card. If no filename is given, stop playing.
sd [<filename>]


# Infra Red signal receiving and transmitting
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

# Compile

To compile the project, we'll need to follow the teensy website [instruction](http://www.pjrc.com/teensy/first_use.html) to install the teensyduino. I'm a heavy vim user, so I also borrowed makefile from [here](http://forum.pjrc.com/threads/23605-Teensy-mk-port-of-Arduino-mk-Makefile). I slightly modified the makefile to make it work with ``cygwin``, and is available [here](https://gist.github.com/realthunder/9374708). You need to modify it to set ``ARDUINO_DIR`` to your arduino installation direction. You probably need to install the arduino in a none spaced directory to make cygwin happy.

You will need [bitlash](http://bitlash.net) to compile the project. You need a patched version from [here](https://github.com/realthunder/bitlash).

To compile and upload, simply use the following command
```
make upload
```
