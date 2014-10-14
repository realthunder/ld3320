ARDUINO_LIBS = SPI SD SD/utility IRremote
OTHER_LIBS = bitlash
ARDUINO_SKETCHBOOK = ../
EXTRA_FLAGS = -DIR_RAWBUF=256 -DBITLASH_INTERNAL -DBITLASH_PROMPT='">\r\n"'
ARDUINO_DIR=../../ide

ifeq ($(board),)

USER_LIBS = $(OTHER_LIBS)
OPTIONS = $(EXTRA_FLAGS) -DLAYOUT_US_ENGLISH -DUSB_SERIAL

include ../Teensy.mk

else

ARDUINO_LIBS += $(OTHER_LIBS)
EXTRA_FLAGS += -DTINY_BUILD -DUSER_FUNCTIONS -DBAUD_RATE=9600 -DARDUINO_TIMER_SCALE=1

ifeq ($(board),pro2)
    override board = pro5v328
	EXTRA_FLAGS += -DBOARD_PRO2
endif
ifeq ($(board),nano3)
    override board = nano328
endif
BOARD_TAG = $(board)

include ../Arduino-Makefile/Arduino.mk

endif
