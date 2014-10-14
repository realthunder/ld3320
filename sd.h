#include <SPI.h>
#include <SD.h>

#define SD_SPEED 2
#define SD_BUFSIZE 4097

File file;
uint32_t fileSize;
uint32_t filePos;
uint32_t sdBufHead;
uint32_t sdBufTail;
uint8_t sdBuf[SD_BUFSIZE];

enum {
    SD_NULL,
    SD_INIT,
    SD_PLAYING,
    SD_ENDING,
};
int sdState;

void sdClaimSPI() {
    if(spiOwner != SPI_OWNER_SD)
        spiOwner = SPI_OWNER_SD;
    SPI.setMOSI(SD_SDI);
    SPI.setMISO(SD_SDO);
    SPI.setSCK(SD_SCK);
    SD.setSckRate(SD_SPEED);
}

boolean sdInit() {
    sdClaimSPI();
    if (!SD.begin(SD_SCS, SD_SPEED)) {
        Serial.println("sd card init failed");
        sdState = SD_NULL;
        return 0;
    }
    sdState = SD_INIT;
    return 1;
}

void setupSD() {
    pinMode(SD_SCS,OUTPUT);
    digitalWrite(SD_SCS,HIGH);
}

void loopSD() {
    int ret;
    uint32_t size;

    if(sdState == SD_ENDING) {
        if(LD_CheckPlayEnd()) {
            sdState = SD_INIT;
            Serial.println("mp3 ended");
        }
        return;
    }

    if(sdState != SD_PLAYING) return;

    while(filePos<fileSize) {
        if(sdBufHead == 0) 
            size = SD_BUFSIZE - sdBufTail - 1;
        else if(sdBufHead <= sdBufTail)
            size = SD_BUFSIZE - sdBufTail;
        else 
            size = sdBufHead - sdBufTail - 1;
        if(size + filePos > fileSize)
            size = fileSize - filePos;
        if(!size) break;

        sdClaimSPI();
        ret = file.read(sdBuf+sdBufTail,size);
        if(ret!=size) {
            Serial.println("file read failed");
            sdState = SD_INIT;
        }
        // Serial.print("read ");
        // Serial.println(size);
        sdBufTail = (sdBufTail+size)%SD_BUFSIZE;
        filePos += size;
    }

    do {
        if(sdBufHead <= sdBufTail)
            size = sdBufTail - sdBufHead;
        else 
            size = SD_BUFSIZE - sdBufHead;
        if(!size) {
            if(filePos == fileSize) sdState = SD_ENDING;
            sdBufTail = sdBufHead = 0;
            break;
        }
        ret = LD_LoadMp3Data(sdBuf+sdBufHead,size);
        if(ret < 0) {
            sdState = SD_INIT;
            return;
        }
        // Serial.print("load ");
        // Serial.println(size);
        sdBufHead = (sdBufHead+ret)%SD_BUFSIZE;
    }while(ret == size);

}

void sdPlay(char *name) {
    int ret;
    uint32_t readSize;

    if(sdState == SD_NULL) 
        if(!sdInit()) return;

    sdClaimSPI();

    if(!SD.exists(name)) {

        Serial.print(name);
        Serial.println(" does not exists");
        return;
    }

    file = SD.open(name,FILE_READ);
    if(!file) {
        Serial.println("failed to open file");
        return;
    }
    fileSize = file.size();
    Serial.print("file size ");
    Serial.println(fileSize);

    if(fileSize < SD_BUFSIZE-1)
        readSize = fileSize;
    else
        readSize = SD_BUFSIZE-1;

    if(file.read(sdBuf,readSize) != readSize) {
        Serial.println("failed to read file");
        return;
    }
    // Serial.print("read ");
    // Serial.println(readSize);

    filePos = readSize;
    sdBufHead = 0;
    sdBufTail = readSize;

    LD_Init_MP3();
	LD_AdjustMIX2SPVolume(SPEAKER_VOL);
    ret = LD_LoadMp3Data(sdBuf,readSize);
    if(ret <= 0) {
        Serial.println("failed to load mp3 data");
        return;
    }
    // Serial.print("load ");
    // Serial.println(ret);

    if(ret == readSize) 
        sdBufTail = 0;
    else
        sdBufHead = ret;
    LD_Play();
    sdState = SD_PLAYING;
}

numvar sdCmd() {
    unsigned n = getarg(0);
    if(n==0) {
        if(sdState == SD_NULL)
            sdInit();
        else {
            sdState = SD_INIT;
            LD_reset();
        }
        return 0;
    }

    if(isstringarg(1)) {
        sdPlay((char*)getstringarg(1));
        return 0;
    }
}

