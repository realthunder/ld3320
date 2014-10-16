#include <SPI.h>
#include <SdFat.h>
#include <Base64.h>

#define SD_SPEED 2
#define SD_BUFSIZE 4097

uint32_t fileSize;
uint32_t filePos;
uint32_t sdBufHead;
uint32_t sdBufTail;
uint8_t sdBuf[SD_BUFSIZE];

#define FNAMELEN 13
char cachedname[FNAMELEN];
byte cachedflags;

SdFat sd;
SdFile sdfile;
SdFile sdMp3;
SdSpi sdspi;

byte sdDirPos[2];
char sdDir[2][64];

#define sdDefaultPos sdDirPos[0]
#define sdCurrentPos sdDirPos[1]
#define sdDefault sdDir[0]
#define sdCurrent sdDir[1]

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
    sdspi.init(SD_SPEED);
}

boolean sdInit() {
    sdClaimSPI();
    if(sdState == SD_NULL) {
        sdClaimSPI();
        if(sd.begin(SD_SCS,SD_SPEED))
            sdState = SD_INIT;
        else {
            Serial.println("sd card init failed");
            sdState = SD_NULL;
            return 0;
        }
    }
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
            Serial.println("ended");
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
        ret = sdMp3.read(sdBuf+sdBufTail,size);
        if(ret!=size) {
            Serial.println("file read failed");
            sdState = SD_INIT;
        }
        sdBufTail = (sdBufTail+size)%SD_BUFSIZE;
        filePos += size;
    }

    do {
        if(sdBufHead <= sdBufTail)
            size = sdBufTail - sdBufHead;
        else 
            size = SD_BUFSIZE - sdBufHead;
        if(!size) {
            if(filePos == fileSize) {
                Serial.println("ending");
                sdState = SD_ENDING;
                LD_Mp3LoadFinish();
                sdMp3.close();
            }
            sdBufTail = sdBufHead = 0;
            break;
        }
        ret = LD_LoadMp3Data(sdBuf+sdBufHead,size);
        if(ret < 0) {
            sdState = SD_INIT;
            return;
        }
        sdBufHead = (sdBufHead+ret)%SD_BUFSIZE;
    }while(ret == size);

}

void sdPlay(char *filename, int wait) {
    char *name = filename;
    int ret;
    uint32_t readSize;

    sdState = SD_INIT;
    LD_reset();

    if(!sdInit()) return;

    sdMp3.close();

    if(filename[0] != '/' && sdCurrentPos) {
        strncpy(sdCurrent+sdCurrentPos,filename,sizeof(sdCurrent)-sdCurrentPos);
        name = sdCurrent;
    }

    if(!sd.exists(name)) {
        if(filename[0] != '/' && sdDefaultPos) {
            strncpy(sdDefault+sdDefaultPos,filename,sizeof(sdDefault)-sdDefaultPos);
            name = sdDefault;
            if(sd.exists(name)) 
                goto NEXT;
        }
        Serial.print("cannot find ");
        Serial.println(filename);
        return;
    }

NEXT:
    if(!sdMp3.open(name)) {
        Serial.print("failed to open ");
        Serial.println(filename);
        return;
    }
    fileSize = sdMp3.fileSize();
    if(fileSize < SD_BUFSIZE-1)
        readSize = fileSize;
    else
        readSize = SD_BUFSIZE-1;

    if(sdMp3.read(sdBuf,readSize) != readSize) {
        Serial.print("failed to read ");
        Serial.print(name);
        return;
    }

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

    if(ret == readSize) 
        sdBufTail = 0;
    else
        sdBufHead = ret;
    LD_Play();
    sdState = SD_PLAYING;

    if(wait) {
        do {
            loopSD();
        } while(sdState==SD_PLAYING || sdState == SD_ENDING);
    }
}

numvar sdCmd() {
    unsigned n = getarg(0);
    numvar arg;
    if(n==0) {
        sdInit();
        sdState = SD_INIT;
        LD_reset();
        return 0;
    }

    if(isstringarg(1)) {
        sdPlay((char*)getstringarg(1),n>1?getarg(2):0);
        return 0;
    }

    arg = getarg(1);
    switch(arg) {
    case 0:
    case 1:
        if(n>1) {
            char *name = (char *)getstringarg(2);
            unsigned len = strlen(name);
            if(len==0 || len+14 >= sizeof(sdDir[arg])) {
                Serial.println("invalid sd dir");
                return 0;
            }
            strcpy(sdDir[arg],name);
            if(name[len-1] != '/')
                sdDir[arg][len++] = '/';
            sdDirPos[arg] = len;
        }else
            sdDirPos[arg] = 0;
        break;
    }
    return 0;
}

// return true iff script exists
byte scriptfileexists(char *scriptname) {
	if (!(sdInit())) return 0;
	return sd.exists(scriptname);
}

// open and set parse location on input file
byte scriptopen(char *scriptname, numvar position, byte flags) {
    sdClaimSPI();
	// open the input file if there is no file open, 
	// or the open file does not match what we want
	if (!sdfile.isOpen() || strcmp(scriptname, cachedname) || (flags != cachedflags)) {
		if (sdfile.isOpen()) sdfile.close();

		if (!sdfile.open(scriptname, flags)) return 0;
		strcpy(cachedname, scriptname);		// cache the name we have open
		cachedflags = flags;				// and the mode
		if (position == 0L) return 1;		// save a seek, when we can
	}
	return sdfile.seekSet(position);
}

numvar scriptgetpos(void) {
	return sdfile.curPosition();
}

byte scriptread(void) {
    sdClaimSPI();
	int input = sdfile.read();
	if (input == -1) {
		//scriptfile.close();		// leave the file open for re-use
		return 0;
	}
	return (byte) input;
}

byte scriptwrite(char *filename, char *contents, byte append) {
    sdClaimSPI();
    size_t len;
	byte flags;
	if (append) flags = O_WRITE | O_CREAT | O_APPEND;
	else 		flags = O_WRITE | O_CREAT | O_TRUNC;

	if (!scriptopen(filename, 0L, flags)) return 0;
	if ((len=strlen(contents))) {
        if(sdfile.write(contents, len) != len) return 0;
	}
	return 1;
}

void scriptwritebyte(byte b) {
    sdClaimSPI();
	sdfile.write(b);
}

numvar sdls(void) {
	if (sdInit()) sd.ls(LS_SIZE);		// LS_SIZE, LS_DATE, LS_R, indent
	return 0;
}
numvar sdexists(void) { 
	if (!sdInit()) return 0;
	return scriptfileexists((char *) getarg(1)); 
}
numvar sdrm(void) { 
	if (!sdInit()) return 0;
	return sd.remove((char *) getarg(1)); 
}
numvar sdappend(void) { 
	if (!sdInit()) return 0;
	return sdwrite((char *) getarg(1), (char *) getarg(2), 1); 
}
numvar sdapped64(void) {
    if(!sdInit()) return 0;
    char *filename = (char*)getstringarg(1);
    char *d = (char *)getstringarg(2);
    unsigned len = strlen(d);
    if(len >= sizeof(sdBuf)) {
        Serial.println("error:overflow");
        return 0;
    }

    len = base64_decode((char*)sdBuf,d,len);
	if (!scriptopen(filename, 0L, O_WRITE|O_CREAT|O_APPEND)) {
        Serial.println("error:open");
        return 0;
    }
    if(sdfile.write(sdBuf, len) != len) {
        Serial.println("error:write");
        return 0;
	}
	return 1;
}
numvar sdcd(char *dir) {
	if (!sdInit()) return 0;
    cachedname[0] = 0;
	return sd.chdir(dir);
}
numvar sdcd(void) {
    sdcd((char*)getstringarg(1));
}
numvar sdmd(void) { 
	if (!sdInit()) return 0;
	return sd.mkdir((char *) getarg(1));
}

