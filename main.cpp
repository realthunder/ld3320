#include <Arduino.h>

#define ENABLE_SHELL
#define IR_NO_RECV

#ifndef BAUD_RATE
#define BAUD_RATE 38400
#endif

#define DECLARE_TIMEOUT static unsigned long _t_start, _t_tick
#define INIT_TIMEOUT unsigned long _t_start = millis(), _t_tick
#define RESET_TIMEOUT _t_start=millis()

// cheap timeout detection, when wrap around, the actual timeout may double
#define IS_TIMEOUT(_timeout) \
    (_t_tick=millis(),\
     (_t_tick>=_t_start&&(_t_tick-_t_start)>=_timeout) || \
        (_t_tick<_t_start&&_t_tick>=_timeout))

// accurate timeout detection
#define IS_TIMEOUT2(_timeout) \
    (_t_tick=millis(),\
     (_t_tick>=_t_start&&(_t_tick-_t_start)>=_timeout) || \
        (_t_tick<_t_start&&((0xffffffff-_t_start)+_t_tick)>=_timeout))

#define noop do{}while(0)

#ifdef BOARD_PRO2
#else
#   define IR_PIN 3
#   define MD_PIN 6
#   define SPIS_PIN 4
#   define RSTB_PIN 5
#   define SCS_PIN ASR_SCS
#   define ASR_SDI 11
#   define ASR_SDO 12
#   define ASR_SCK 13
#   define ASR_SCS 10
#   define SD_SDI 7
#   define SD_SDO 8
#   define SD_SCK 14
#   define SD_SCS 2
#endif

#include <bitlash.h>

enum {
    SPI_OWNER_NONE,
    SPI_OWNER_ASR,
    SPI_OWNER_SD,
};
int spiOwner;

#include "ir.h"
#include "asr.h"
#include "sd.h"

void setupShell() {
    initBitlash(BAUD_RATE);
    addBitlashFunction("asr",(bitlash_function) asrCmd);
    addBitlashFunction("sd",(bitlash_function) sdCmd);
    addBitlashFunction("ir",(bitlash_function) irCmd);
}

void loopShell() {
    runBitlash();
}
void setup() {
    setupShell();
    setupAsr();
    setupSD();
    setupIR();
}

void loop() {
    loopShell();
    loopAsr();
    loopSD();
    loopIR();
}

