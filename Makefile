ARDUINO_LIBS = SPI IRremote
OTHER_LIBS = bitlash SdFat-beta/SdFat SdFat-beta/SdFat/utility base64
ARDUINO_SKETCHBOOK = ../
EXTRA_FLAGS = -DIR_RAWBUF=256 -DBITLASH_INTERNAL -DBITLASH_PROMPT='">\r\n"' -DSDFILE
ARDUINO_DIR=../../ide

ifneq ($(board),)
	BOARD=$(board)
else
	BOARD=teensy3
endif


ifeq ($(BOARD),teensy3)

USER_LIBS = $(OTHER_LIBS)
OPTIONS = $(EXTRA_FLAGS) -DLAYOUT_US_ENGLISH -DUSB_SERIAL

include ../Teensy.mk

else

ARDUINO_LIBS += $(OTHER_LIBS)
EXTRA_FLAGS += -DTINY_BUILD -DUSER_FUNCTIONS -DBAUD_RATE=9600 -DARDUINO_TIMER_SCALE=1

ifeq ($(BOARD),pro2)
    BOARD = pro5v328
	EXTRA_FLAGS += -DBOARD_PRO2
endif
ifeq ($(BOARD),nano3)
    BOARD = nano328
endif
BOARD_TAG = $(BOARD)

include ../Arduino-Makefile/Arduino.mk

endif
