#include <IRremote.h>
#define CODE_5104(u,s) ((3<<10)|(u<<7)|s)
#define T_5104 1688 /* T=1.6879ms */
#define T2_5104 (T_5104/4)

#define IR_5104 -2

#ifndef IR_NO_RECV
IRrecv irrecv(RECV_PIN);
decode_results results;
#endif
IRsend irsend;

void setupIR() {
#ifndef ENABLE_SHELL
    irrecv.enableIRIn();
#endif
}

void send5104(unsigned user,unsigned code /* 1~8 for K1~K8 */) {
    int i;
    static const unsigned code_5104[] = {1,2,4,8,0x10,0x20,0x43,0x46};
    static const int tick_5104[] = {T2_5104,3*T2_5104};
    code = CODE_5104(user,code_5104[code-1]);
    irsend.enableIROut(38);
    for(i=11;i>=0;--i) {
        unsigned c = (code>>i)&1;
        irsend.mark(tick_5104[c]);
        irsend.space(tick_5104[c^1]);
    }
    irsend.space(4*T_5104);

}

int toggle = 0; // The RC5/6 toggle state
unsigned int rawBuf[RAWBUF];
int rawInit;
int codeType=-1;
unsigned long codeValue;
int codeLen;

#ifdef ENABLE_SHELL
// Stores the code for later playback
// Most of this code is just logging
void irDecode(decode_results *results,int width) {
    int type = results->decode_type;
    int len;
    if (type == UNKNOWN) {
        unsigned int *buf = rawBuf;
        len = results->rawlen - 1;
        // To store raw codes:
        // Drop first value (gap)
        // Convert from ticks to microseconds
        // Tweak marks shorter, and spaces longer to cancel out IR receiver distortion
        sp("ir(2");
        for (int i = 1; i <= len; i++) {
            unsigned int code = results->rawbuf[i];
            if(i>1 && width && (i-1)%width == 0) {
                sp(")");
                speol();
                sp("ir(3");
            }
            sp(",");
            printInteger(code,0,0);
            *buf++ = code;
        }
        sp(")\r\nir(1,");
        printInteger(type,0,0);
        sp(")\r\n");
        codeType = type;
        codeLen = len;
        rawInit = 0;
        return;
    } else if (type == NEC && results->value == REPEAT) {
        // Don't record a NEC repeat value as that's useless.
        sp("REPEAT\r\n");
        return;
    }
    sp("ir(1,");
    printInteger(type,0,0);
    sp(",");
    printInteger(results->bits,0,0);
    sp(",0x");
    printHex(results->value);
    sp(")\r\n");
    codeLen = results->bits;
    codeValue = results->value;
}

int irSend(int repeat, bool trace_raw=false) {
    int i;
    int ret = -1;
    if(codeType == UNKNOWN /* i.e. raw */) {
        sp("Sent RAW");
        // Assume 38 KHz
        if(!rawInit) {
            for(i=0;i<codeLen;++i) {
                unsigned int code = rawBuf[i]*USECPERTICK;
                if(i&1) 
                    code += MARK_EXCESS; //space;
                else
                    code -= MARK_EXCESS; //mark
                rawBuf[i] = code;
            }
            rawInit=1;
        }
        if(trace_raw) {
            for(i=0;i<codeLen;++i) {
                if((i&15)== 0)
                    speol();
                printInteger(rawBuf[i],0,0);
                sp(",");
            }
        }
        irsend.sendRaw(rawBuf, codeLen, 38);
    }else if (codeType == NEC) {
        if (repeat) {
            irsend.sendNEC(REPEAT, codeLen);
            sp("Sent NEC REPEAT");
        } 
        else {
            irsend.sendNEC(codeValue, codeLen);
            sp("Sent NEC ");
            printInteger(codeLen,0,0);
            sp(",");
            printHex(codeValue);
        }
    } else if (codeType == SONY) {
        irsend.sendSony(codeValue, codeLen);
        sp("Sent SONY ");
        printInteger(codeLen,0,0);
        sp(",");
        printHex(codeValue);
    } else if (codeType == RC5 || codeType == RC6) {
        if (!repeat) {
            // Flip the toggle bit for a new button press
            toggle = 1 - toggle;
        }
        // Put the toggle bit into the code to send
        codeValue &= ~(1 << (codeLen - 1));
        codeValue |= (toggle << (codeLen - 1));
        if (codeType == RC5) {
            sp("Sent RC5 ");
            printInteger(codeLen,0,0);
            sp(",");
            printHex(codeValue);
            irsend.sendRC5(codeValue, codeLen);
        } 
        else {
            irsend.sendRC6(codeValue, codeLen);
            sp("Sent RC6 ");
            printInteger(codeLen,0,0);
            sp(",");
            printHex(codeValue);
        }
    }else if(codeType == IR_5104) {
        sp("Sent 5104 ");
        printInteger(codeLen,0,0);
        sp(",");
        printInteger(codeValue,0,0);
        send5104(codeLen,codeValue);
    }else{
        sp("Invalid type");
        ret = -1;
    }
    speol();
    return ret;
}

#ifndef IR_NO_RECV
// ir(0, <column width>, <wait period in ms>)
numvar irCmdRecv(unsigned n) {
    int width = n>1?getarg(2):16;
    unsigned long timeout = (n>2?getarg(3)*1000:5000);
    if(timeout>30000) timeout=30000;
    INIT_TIMEOUT;

    irrecv.enableIRIn();
    while(1) {
        delay(100);
        if(irrecv.decode(&results)) {
            digitalWrite(STATUS_PIN, HIGH);
            irDecode(&results,width);
            digitalWrite(STATUS_PIN, LOW);
            break;
        }
        if(IS_TIMEOUT(timeout)) {
            sp("Timeout\r\n");
            return -1;
        }
    }
    return 0;
}
#else
numvar irCmdRecv(unsigned n) {
    Serial.println("ir recv disabled");
    return 0;
}
#endif

// ir(1,[type],<len>,<value>)
numvar irCmdSend(unsigned n, bool trace_raw) {
    codeType = getarg(2);
    if(codeType != UNKNOWN) {
        codeLen = getarg(3);
        codeValue = getarg(4);
    }
    irSend(0,trace_raw);
    return 0;
}

// ir(2,[values...])  reset codeLen=0
// ir(3,[values...])
numvar irCmdSetRaw(unsigned n) {
    unsigned i;
    if(codeLen+n-1>=RAWBUF) {
        sp("out of boundary");
        speol();
        return -1;
    }
    for(i=2;i<=n;++i) 
        rawBuf[codeLen++] = getarg(i);
    rawInit = 0;
    return 0;
}

numvar irCmd() {
    unsigned n = getarg(0);
    if(n==0) {
        irSend(0);
        return 0;
    }
    switch(getarg(1)) {
    case 0:
        return irCmdRecv(n);
    case 1:
        return irCmdSend(n,false);
    case 4: 
        return irCmdSend(n,true);
    case 2:
        codeLen=0;
        //fall through
    case 3:
        return irCmdSetRaw(n);
    default:
        return -1;
    }
}

#endif

void loopIR() {
#ifndef ENABLE_SHELL
    INIT_TIMEOUT;

    static unsigned long timeout = 0;
    static byte last_mt = 0x30, current_mt;
    static byte last_sv = 48;

    // toggle button to manually turn on/off fan
    if(buttonState != lastButtonState) {
        unsigned v;
        if(current_mt == 0) {
            v = last_sv;
            current_mt = last_mt;
            if(timeout) RESET_TIMEOUT;
        }else {
            v = 0;
            current_mt = 0;
        }
        mt(1,current_mt,0,0);
        sv(2,1,v);
    }

    if(current_mt && timeout && IS_TIMEOUT(timeout)) {
        current_mt = 0;
        mt(1,0,0,0);
        sv(2,1,0);
    }

    if (irrecv.decode(&results)) {
        digitalWrite(STATUS_PIN, HIGH);
        Serial.print("ir ");
        Serial.print(results.decode_type);
        Serial.print(", ");
        Serial.println(results.value,HEX);
        if(results.decode_type == RC5) {
            unsigned cmd = (results.value>>8) & 0xf;
            results.value &= 0xff;
            switch(cmd) {
            case 0: { //change motor speed and servo swing speed
                unsigned v = results.value&0xf0;
                if(v == 0xf0) v = 0xff;
                if(!current_mt && v && timeout)
                    RESET_TIMEOUT;
                current_mt = v;
                if(v) last_mt = v;
                mt(1,v,0,0);

                v = (results.value&0xf)<<4;
                if(v) last_sv = v;
                sv(2,1,v);
                break;
            } case 1: { //change servo swing range
                sv(2,3,results.value);
                break;
            } case 2: { //change timeout in minutes
                timeout = results.value*60000;
                if(current_mt) RESET_TIMEOUT;
                break;
            } default:
                break;
            }
        }
        irrecv.resume(); // resume receiver
        digitalWrite(STATUS_PIN, LOW);
    }
#endif
}

