#include <SPI.h>
#include "AsrItem.h"  

#define TRACE_ASR_STATE 0

	#define uint8  unsigned char
	#define uint16 unsigned int
	#define uint32 unsigned long   	 	
								
	#define CLK_IN   		    22.1184	//  用户可以根据提供给LD3320模块的实际晶振频率自行修改
	#define LD_PLL_11			(uint8)((CLK_IN/2.0)-1)

	#define LD_PLL_ASR_19 		(uint8)(CLK_IN*32.0/(LD_PLL_11+1) - 0.51)
	#define LD_PLL_ASR_1B 		0x48
	#define LD_PLL_ASR_1D 		0x1f

	#define LD_PLL_MP3_19		0x0f
	#define LD_PLL_MP3_1B		0x18
	#define LD_PLL_MP3_1D   	(uint8)(((90.0*((LD_PLL_11)+1))/(CLK_IN))-1)

	// LD chip fixed values.
	#define RESUM_OF_MUSIC              0x01
	#define CAUSE_MP3_SONG_END          0x20
	
	#define MASK_INT_SYNC				0x10
	#define MASK_INT_FIFO				0x04
	#define MASK_AFIFO_INT				0x01
	#define MASK_FIFO_STATUS_AFULL		0x08

	/****************************************************************
	函数名： LD_WriteReg
	功能：写LD3320芯片的寄存器
	参数：  address, 8位无符号整数，地址
			dataout，8位无符号整数，要写入的数据
	返回值：无
	****************************************************************/ 
	void LD_WriteReg( unsigned char address, unsigned char dataout );
	
	/****************************************************************
	函数名： LD_ReadReg
	功能：读LD3320芯片的寄存器
	参数：  address, 8位无符号整数，地址
	返回值：8位无符号整数，读取的结果
	****************************************************************/ 
	unsigned char LD_ReadReg( unsigned char address );	
	void LD_reset();
	
	void LD_Init_Common(int mp3);	  
	void LD_Init_ASR();	  

	void LD_AdjustMIX2SPVolume(uint8 value);  	
	uint8 LD_AsrRun();
	uint8 LD_AsrAddFixed();
	uint8 LD_GetResult();

    void ProcessInt0();
	uint8 RunASR();
	extern void _nop_ (void);

	void MCU_init();   
	void  ProcessInt0();
	void  test_led(void);

//////////////////////////////////////////////////////////////////////
#define asrSetState(_name) \
    do{ \
        if(TRACE_ASR_STATE) {\
            Serial.print(__LINE__);\
            Serial.print(": ");\
            Serial.print(asrStateName[asrState]);\
            Serial.println("->" #_name);\
        }\
        asrState = LD_ASR_##_name;\
    }while(0)

enum {
    LD_ASR_NONE,
    LD_ASR_INIT,
    LD_ASR_RUNNING,
    LD_ASR_FOUNDOK,
    LD_ASR_FOUNDZERO,
    LD_ASR_ERROR,
};
int asrState;
const char *asrStateName[] = {
    "none",
    "init",
    "running",
    "ok",
    "zero",
    "error",
};
int asrResult;
int asrIndex;
char asrTable[50][2][14];
char asrDir[4][64] = { {"/menu"} };
char asrFile[4][13] = { {"main"} };
#define asrCurrent asrFile[0]
#define asrCurrentDir asrDir[0]
#define asrDefault asrFile[1]
#define asrDefaultDir asrDir[1]
#define asrError asrFile[2]
#define asrErrorDir asrDir[2]
#define asrConfirm asrFile[3]
#define asrConfirmDir asrDir[3]

#define SPI_TR SPI.transfer
#define SET_PIN(_name) digitalWrite(_name##_PIN,HIGH)
#define CLR_PIN(_name) digitalWrite(_name##_PIN,LOW)

void asrClaimSPI() {
    if(spiOwner != SPI_OWNER_ASR)
        spiOwner = SPI_OWNER_ASR;
    SPI.setMOSI(ASR_SDI);
    SPI.setMISO(ASR_SDO);
    SPI.setSCK(ASR_SCK);
    SPI.setBitOrder(MSBFIRST);
    SPI.setDataMode(SPI_MODE1);
    SPI.setClockDivider(SPI_CLOCK_DIV8);
}

void LD_WriteReg( unsigned char address, unsigned char dataout )
{
    CLR_PIN(SCS);
    SPI_TR(0x04); // 发送 0x04
    SPI_TR(address); // 发送 address
    SPI_TR(dataout); // 发送 dataout
    SET_PIN(SCS);
}
unsigned char LD_ReadReg( unsigned char address )
{
    unsigned char ret;
    CLR_PIN(SCS);
    SPI_TR(0x05); // 发送 0x05
    SPI_TR(address); // 发送 address
    ret = SPI_TR(0); // 读出数据, 并返回
    SET_PIN(SCS);
    return ret;
}

/************************************************************************************/
//	这个C文件里面包含的函数，相当于LD3320的驱动，以C语言源代码的形式提供
//	开发者在没有调试通过之前，一定不要修改这里面的函数
//	
//	LD_ReloadMp3Data()函数由于牵涉到主控MCU向外部存储芯片读取MP3数据的操作
//	所以开发者应该根据自己的实际使用的存储芯片，去修改这个函数
//	但是要保证对于LD3320芯片的操作不改变
//
//	LD_GetResult()函数目前只简单取了第一候选结果作为识别结果
//	开发者应该根据自己产品设计，决定是否要读取其他的识别候选结果
/************************************************************************************/		
void LD_reset()
{
    SET_PIN(LED);

	SET_PIN(RSTB);
	delay(1);
	CLR_PIN(RSTB);
	delay(1);
	SET_PIN(RSTB);

	delay(1);
	CLR_PIN(SCS);
	delay(1);
	SET_PIN(SCS);
	delay(1);

    asrSetState(NONE);
    asrIndex = 0;
}

void LD_Init_Common(int mp3)
{ 
	LD_ReadReg(0x06);  
	LD_WriteReg(0x17, 0x35); 
	delay(10);
	LD_ReadReg(0x06);  

	LD_WriteReg(0x89, 0x03);  
	delay(5);
	LD_WriteReg(0xCF, 0x43);   
	delay(5);
	LD_WriteReg(0xCB, 0x02);
	
	/*PLL setting*/
	LD_WriteReg(0x11, LD_PLL_11);       
	
	if (mp3)
	{
		LD_WriteReg(0x1E, 0x00); 
		LD_WriteReg(0x19, LD_PLL_MP3_19);   
		LD_WriteReg(0x1B, LD_PLL_MP3_1B);   
		LD_WriteReg(0x1D, LD_PLL_MP3_1D);
	}
	else
	{
		LD_WriteReg(0x1E,0x00);
		LD_WriteReg(0x19, LD_PLL_ASR_19); 
		LD_WriteReg(0x1B, LD_PLL_ASR_1B);		
	    LD_WriteReg(0x1D, LD_PLL_ASR_1D);
	}

	delay(10);
	
	LD_WriteReg(0xCD, 0x04);
	LD_WriteReg(0x17, 0x4C); 
	delay(5);
	LD_WriteReg(0xB9, 0x00);
	LD_WriteReg(0xCF, 0x4F); 
	LD_WriteReg(0x6F, 0xFF); 
} 

void LD_Init_MP3()
{
    asrClaimSPI();
	LD_Init_Common(1);

	LD_WriteReg(0xBD,0x02);
	LD_WriteReg(0x17, 0x48);
	delay(10);

	LD_WriteReg(0x85, 0x52); 
	LD_WriteReg(0x8F, 0x00);  
	LD_WriteReg(0x81, 0x00);
	LD_WriteReg(0x83, 0x00);
	LD_WriteReg(0x8E, 0xff);
	LD_WriteReg(0x8D, 0xff);
    delay(1);
	LD_WriteReg(0x87, 0xff);
	LD_WriteReg(0x89, 0xff);
	delay(1);
	LD_WriteReg(0x22, 0x00);    
	LD_WriteReg(0x23, 0x00);
	LD_WriteReg(0x20, 0xef);    
	LD_WriteReg(0x21, 0x07);
	LD_WriteReg(0x24, 0x77);          
    LD_WriteReg(0x25, 0x03);
    LD_WriteReg(0x26, 0xbb);    
    LD_WriteReg(0x27, 0x01); 
}

void LD_Play() {
    asrClaimSPI();
    LD_WriteReg(0xBA, 0x00);
    LD_WriteReg(0x17, 0x48);
    LD_WriteReg(0x33, 0x01);
    LD_WriteReg(0x29, 0x04);

    LD_WriteReg(0x02, 0x01); 
    LD_WriteReg(0x85, 0x5A);
}

DECLARE_TIMEOUT;

void LD_Mp3LoadFinish() {
    asrClaimSPI();
    LD_WriteReg(0xBC, 0x01);
    LD_WriteReg(0x29, 0x10);
    LD_WriteReg(0x02, 0x0);
    delay(5);
    return;
}

boolean LD_CheckPlayEnd() {
    //The following code always need to wait for 4 second 
    //before detecting the end. Not really useful here.
    asrClaimSPI();
    byte v = LD_ReadReg(0xBA);
    byte n = digitalRead(INT_PIN);
    Serial.print(v);
    Serial.print(',');
    Serial.println(n);
    delay(1000);
    if(v&CAUSE_MP3_SONG_END) {
        LD_WriteReg(0x2B, 0);
        LD_WriteReg(0xBA, 0);	
        LD_WriteReg(0xBC,0x0);	
        LD_WriteReg(0x08,1);
        delay(5);
        LD_WriteReg(0x08,0);
        LD_WriteReg(0x33, 0);	 
        return 1;
    }
    return 0;
}

int LD_LoadMp3Data(uint8_t *data, unsigned size) {
    int ret;
    asrClaimSPI();
	for(ret=0;size!=ret && !(LD_ReadReg(0x06)&MASK_FIFO_STATUS_AFULL);++ret)
		LD_WriteReg(0x01,*data++);
    return ret;
}

void LD_Init_ASR()
{	
    asrClaimSPI();
	LD_Init_Common(0);

	LD_WriteReg(0xBD, 0x00);
	LD_WriteReg(0x17, 0x48);
	delay( 10 );

	LD_WriteReg(0x3C, 0x80);    
	LD_WriteReg(0x3E, 0x07);
	LD_WriteReg(0x38, 0xff);    
	LD_WriteReg(0x3A, 0x07);
	
	LD_WriteReg(0x40, 0);          
	LD_WriteReg(0x42, 8);
	LD_WriteReg(0x44, 0);    
	LD_WriteReg(0x46, 8); 
	delay( 1 );

    asrSetState(INIT);
}


/*
void ProcessInt0()	  //收到语音识别结果，进入外部中断处理函数
{
	uint8 nAsrResCount=0;

	EX0=0;			 //关闭外部中断，处理完数据后重新开启
	
	ucRegVal = LD_ReadReg(0x2B);

		// 语音识别产生的中断
		// （有声音输入，不论识别成功或失败都有中断）
		LD_WriteReg(0x29,0) ;
		LD_WriteReg(0x02,0) ;
		if((ucRegVal & 0x10) &&	LD_ReadReg(0xb2)==0x21 && LD_ReadReg(0xbf)==0x35)
		{
			nAsrResCount = LD_ReadReg(0xba);
			if(nAsrResCount>0 && nAsrResCount<=4) 
			{
				nAsrStatus=LD_ASR_FOUNDOK;
			}
			else
		    {
				nAsrStatus=LD_ASR_FOUNDZERO;
			}	
		}
		else
		{
			nAsrStatus=LD_ASR_FOUNDZERO;
		}
			
		LD_WriteReg(0x2b, 0);
    	LD_WriteReg(0x1C,0);
		
		

	delay(10);
	EX0=1; //处理完外部数据，重新允许外部中断	
	return;  	
}
*/
void LD_AdjustMIX2SPVolume(uint8 val)
{
    asrClaimSPI();
	val = ((15-val)&0x0f) << 2;
	LD_WriteReg(0x8E, val | 0xc3); 
	LD_WriteReg(0x87, 0x78); 
}

// Return 1: success.
uint8 LD_Check_ASRBusyFlag_b2()
{
	uint8 j;
	uint8 flag = 0;
	for (j=0; j<10; j++)
	{
		if (LD_ReadReg(0xb2) == 0x21)
		{
			flag = 1;
			break;
		}
		delay(10);		
	}
	return flag;
}

// Return 1: success.
uint8 LD_AsrRun()
{
    asrClaimSPI();
	LD_WriteReg(0x35, MIC_VOL);
    LD_WriteReg(0xB3, 0x05);	// 用户阅读 开发手册 理解B3寄存器的调整对于灵敏度和识别距离的影响	
							    // 配合MIC，越大越灵敏
	LD_WriteReg(0x1C, 0x09);
	LD_WriteReg(0xBD, 0x20);
	LD_WriteReg(0x08, 0x01);
	delay( 1 );
	LD_WriteReg(0x08, 0x00);
	delay( 1 );

	if(LD_Check_ASRBusyFlag_b2() == 0)
	{
		return 0;
	}

	LD_WriteReg(0xB2, 0xff);	
	LD_WriteReg(0x37, 0x06);
	delay( 5 );
	LD_WriteReg(0x1C, 0x0b);
	LD_WriteReg(0x29, 0x10);
	
	LD_WriteReg(0xBD, 0x00);
    asrSetState(RUNNING);
    CLR_PIN(LED);
	return 1;
}

uint8 LD_AsrPoll() {
    uint8 ret;
    asrClaimSPI();
    if(LD_ReadReg(0xb2)!=0x21) return 0x80; //busy
    if(LD_ReadReg(0xbf)!=0x35) return 0x81; //error
    ret = LD_ReadReg(0xba);
    if(ret > 4) ret = 0;
    return ret;
}

boolean LD_AsrAddFixed_ByString(char * pRecogString, uint8 k)
{
	uint8 nAsrAddLength;

	if (*pRecogString==0)
		return 0;

    asrClaimSPI();
    if(LD_Check_ASRBusyFlag_b2() == 0)
        return 0;

	LD_WriteReg(0xc1, k );
	LD_WriteReg(0xc3, 0 );
	LD_WriteReg(0x08, 0x04);
	delay(1);
	LD_WriteReg(0x08, 0x00);
	delay(1);	

	for (nAsrAddLength=0; nAsrAddLength<50; nAsrAddLength++)
	{
		if (pRecogString[nAsrAddLength] == 0)
			break;
		LD_WriteReg(0x5, pRecogString[nAsrAddLength]);
	}
	
	LD_WriteReg(0xb9, nAsrAddLength);
	LD_WriteReg(0xb2, 0xff);
	LD_WriteReg(0x37, 0x04);
    return 1;
}

void LD_AsrAddFixed_ByIndex(uint8 nIndex)
{
	switch(nIndex)
	{
		case  0: LD_AsrAddFixed_ByString(STR_00,nIndex); break;
		case  1: LD_AsrAddFixed_ByString(STR_01,nIndex); break;
		case  2: LD_AsrAddFixed_ByString(STR_02,nIndex); break;
		case  3: LD_AsrAddFixed_ByString(STR_03,nIndex); break;
		case  4: LD_AsrAddFixed_ByString(STR_04,nIndex); break;
		case  5: LD_AsrAddFixed_ByString(STR_05,nIndex); break;
		case  6: LD_AsrAddFixed_ByString(STR_06,nIndex); break;
		case  7: LD_AsrAddFixed_ByString(STR_07,nIndex); break;
		case  8: LD_AsrAddFixed_ByString(STR_08,nIndex); break;
		case  9: LD_AsrAddFixed_ByString(STR_09,nIndex); break;
		case 10: LD_AsrAddFixed_ByString(STR_10,nIndex); break;
		case 11: LD_AsrAddFixed_ByString(STR_11,nIndex); break;
		case 12: LD_AsrAddFixed_ByString(STR_12,nIndex); break;
		case 13: LD_AsrAddFixed_ByString(STR_13,nIndex); break;
		case 14: LD_AsrAddFixed_ByString(STR_14,nIndex); break;
		case 15: LD_AsrAddFixed_ByString(STR_15,nIndex); break;
		case 16: LD_AsrAddFixed_ByString(STR_16,nIndex); break;
		case 17: LD_AsrAddFixed_ByString(STR_17,nIndex); break;
		case 18: LD_AsrAddFixed_ByString(STR_18,nIndex); break;
		case 19: LD_AsrAddFixed_ByString(STR_19,nIndex); break;
		case 20: LD_AsrAddFixed_ByString(STR_20,nIndex); break;
		case 21: LD_AsrAddFixed_ByString(STR_21,nIndex); break;
		case 22: LD_AsrAddFixed_ByString(STR_22,nIndex); break;
		case 23: LD_AsrAddFixed_ByString(STR_23,nIndex); break;
		case 24: LD_AsrAddFixed_ByString(STR_24,nIndex); break;
		case 25: LD_AsrAddFixed_ByString(STR_25,nIndex); break;
		case 26: LD_AsrAddFixed_ByString(STR_26,nIndex); break;
		case 27: LD_AsrAddFixed_ByString(STR_27,nIndex); break;
		case 28: LD_AsrAddFixed_ByString(STR_28,nIndex); break;
		case 29: LD_AsrAddFixed_ByString(STR_29,nIndex); break;
		case 30: LD_AsrAddFixed_ByString(STR_30,nIndex); break;
		case 31: LD_AsrAddFixed_ByString(STR_31,nIndex); break;
		case 32: LD_AsrAddFixed_ByString(STR_32,nIndex); break;
		case 33: LD_AsrAddFixed_ByString(STR_33,nIndex); break;
		case 34: LD_AsrAddFixed_ByString(STR_34,nIndex); break;
		case 35: LD_AsrAddFixed_ByString(STR_35,nIndex); break;
		case 36: LD_AsrAddFixed_ByString(STR_36,nIndex); break;
		case 37: LD_AsrAddFixed_ByString(STR_37,nIndex); break;
		case 38: LD_AsrAddFixed_ByString(STR_38,nIndex); break;
		case 39: LD_AsrAddFixed_ByString(STR_39,nIndex); break;
		case 40: LD_AsrAddFixed_ByString(STR_40,nIndex); break;
		case 41: LD_AsrAddFixed_ByString(STR_41,nIndex); break;
		case 42: LD_AsrAddFixed_ByString(STR_42,nIndex); break;
		case 43: LD_AsrAddFixed_ByString(STR_43,nIndex); break;
		case 44: LD_AsrAddFixed_ByString(STR_44,nIndex); break;
		case 45: LD_AsrAddFixed_ByString(STR_45,nIndex); break;
		case 46: LD_AsrAddFixed_ByString(STR_46,nIndex); break;
		case 47: LD_AsrAddFixed_ByString(STR_47,nIndex); break;
		case 48: LD_AsrAddFixed_ByString(STR_48,nIndex); break;
		case 49: LD_AsrAddFixed_ByString(STR_49,nIndex); break;	 
	}		
}


// Return 1: success.
//	添加识别关键词语，开发者可以学习"语音识别芯片LD3320高阶秘籍.pdf"中关于垃圾词语吸收错误的用法
uint8 LD_AsrAddFixed()
{
	uint8 k, flag; 			

	flag = 1;
	for (k=0; k<ITEM_COUNT; k++)
	{	 			
		if(LD_Check_ASRBusyFlag_b2() == 0)
		{
			flag = 0;
			break;
		}		
		LD_AsrAddFixed_ByIndex(k);
	}
    return flag;
}	   

uint8 LD_GetResult()
{
	uint8 res;

	res = LD_ReadReg(0xc5);	  	

	return res;
}
/************************************************************************************/
//	RunASR()函数实现了一次完整的ASR语音识别流程
//	LD_AsrStart() 函数实现了ASR初始化
//	LD_AsrAddFixed() 函数实现了添加关键词语到LD3320芯片中
//	LD_AsrRun()	函数启动了一次ASR语音识别流程
//
//	任何一次ASR识别流程，都需要按照这个顺序，从初始化开始进行
/************************************************************************************/

uint8 RunASR()
{
	uint8 i=0;

    asrClaimSPI();

	for (i=0; i<5; i++)			//	防止由于硬件原因导致LD3320芯片工作不正常，所以一共尝试5次启动ASR识别流程
	{
        Serial.println("asr start");
		LD_Init_ASR();
		delay(100);
		if (LD_AsrAddFixed()==0)
		{
            Serial.println("asr add fixed failed");
			LD_reset();			//	LD3320芯片内部出现不正常，立即重启LD3320芯片
			delay(100);			//	并从初始化开始重新ASR识别流程
			continue;
		}
		delay(10);
		if (LD_AsrRun() == 0)
		{
            Serial.println("asr run failed");
			LD_reset();			//	LD3320芯片内部出现不正常，立即重启LD3320芯片
			delay(100);			//	并从初始化开始重新ASR识别流程
			continue;
		}
        Serial.println("asr running");
        return 1;
	}
    return 0;
}

void asrExec(char *name) {
    if(asrState!=LD_ASR_ERROR)
        asrSetState(NONE);
    if(name[0] == '#') {
        Serial.print("play: ");
        Serial.println(name);
        sdPlay(name+1,1);
    }else{
        Serial.print("exec: ");
        Serial.println(name);
        doCommand(name);
    }
}

void asrExec(char *dir, char *name) {
    if(dir && dir[0] && !sdcd(dir)) {
        Serial.print("error: exec cd");
        Serial.println(dir);
        return;
    }
    Serial.print("dir: ");
    Serial.println(dir);
    asrExec(name);
}

bool LD_Confirm() {
    Serial.println("wait confirm");
    if(asrConfirm[0]) 
        asrExec(asrConfirmDir,asrConfirm);
    else {
        LD_reset();
        delay(100);
        LD_Init_ASR();
        LD_AsrAddFixed_ByString("hao wa",0);
        LD_AsrAddFixed_ByString("mei cuo",0);
        LD_AsrAddFixed_ByString("bu shi la",1);
        LD_AsrAddFixed_ByString("bu shi",1);
        LD_AsrAddFixed_ByString("a",2);
        asrTable[0][0][0] = '!';
        asrTable[1][0][0] = '!';
        asrTable[2][0][0] = 0;
        asrTable[3][0][0] = 0;
        asrTable[4][0][0] = 0;
    }
    if(!LD_AsrRun()) {
        LD_reset();
        return false;
    }
    RESET_TIMEOUT;
    int ret;
    while((ret=LD_AsrPoll()) == 0x80) {
        delay(10);
        if(IS_TIMEOUT(5000)) {
            LD_reset();
            return false;
        }
    }
    if(ret == 1) {
        ret = LD_ReadReg(0xc5);
        Serial.print("confirm: ");
        Serial.println(ret);
        return asrTable[ret][0][0] == '!';
    }
    Serial.print("no confirm: ");
    Serial.println(ret);
    return false;
}

numvar asrCmd() {
    unsigned cmd;
    unsigned arg;
    unsigned n = getarg(0);
    if(n==0) {
        LD_reset();
        delay(100);
        LD_Init_ASR();
        return 1;
    }

    cmd = getarg(1);
    switch(cmd) {
    case 0:
        arg=getarg(2);
        if(arg>3) {
            Serial.println("error");
            return 0;
        }
        if(n>2) {
            char *name= (char*)getstringarg(n);
            unsigned len = strlen(name);
            if(len+1>sizeof(asrFile[arg])) {
                Serial.println("error:overflow");
                return 0;
            }
            strcpy(asrFile[arg],name);
            if(n>3) {
                char *dir= (char*)getstringarg(n-1);
                len = strlen(dir);
                if(len+1>sizeof(asrDir[arg])) {
                    Serial.println("error:overflow");
                    return 0;
                }
                strcpy(asrDir[arg],dir);
            }else
                asrDir[arg][0] = 0;
        }else if(asrFile[arg])
            asrExec(asrDir[arg],asrFile[arg]);
        return 1;
    case 1:
        if(!LD_AsrRun()) {
            Serial.println("error:run");
            LD_reset();
            delay(100);
            LD_Init_ASR();
            return 0;
        }
        return 1;
    case 2:
    case 3:
        if(asrState == LD_ASR_INIT) {
            char *command = (char*)getstringarg(2);
            if(asrIndex >= 49) {
                Serial.println("error:overflow");
                return 0;
            }
            LD_AsrAddFixed_ByString(command,asrIndex);
            asrTable[asrIndex][0][0] = 0;
            asrTable[asrIndex][1][0] = 0;
            if(n>2) {
                char *action = (char*)getstringarg(3);
                unsigned len = strlen(action);
                if(len >= sizeof(asrTable[asrIndex][0])) {
                    Serial.println("error:action overlfow");
                    return 0;
                }
                strcpy(asrTable[asrIndex][0],action);
                if(cmd == 3) {
                    if(n>3)
                        strncpy(asrTable[asrIndex][1],(char*)getstringarg(4),sizeof(asrTable[asrIndex][1]));
                    else if(len>8) {
                            Serial.println("error:voice overflow");
                            return 0;
                    }else{
                        strcpy(asrTable[asrIndex][1],action);
                        strcpy(asrTable[asrIndex][1]+len,".mp3");
                    }
                }
            };
            ++asrIndex;
            return 1;
        }else {
            Serial.println("error:state");
            return 0;
        }
        break;
    }
    return 0;
}

void setupAsr() {
    pinMode(MD_PIN,OUTPUT);
    pinMode(SPIS_PIN,OUTPUT);
    pinMode(RSTB_PIN,OUTPUT);
    pinMode(SCS_PIN,OUTPUT);
    pinMode(LED_PIN,OUTPUT);

    SET_PIN(LED);
    SET_PIN(MD);
    CLR_PIN(SPIS);
    SET_PIN(SCS);

    SPI.begin();
    asrClaimSPI();
    LD_reset();
}

void loopAsr() {
    unsigned char ret;

    switch(asrState) {
    case LD_ASR_ERROR:
        if(asrError[0]) {
            asrExec(asrErrorDir,asrError);
            break;
        }
        //fall through
    case LD_ASR_NONE:
        if(asrCurrent[0])
            asrExec(asrCurrentDir,asrCurrent);
        break;
    case LD_ASR_RUNNING:
        ret = LD_AsrPoll();
        if(ret == 0x80) {
            delay(1);
            return;
        }
        if(ret == 1) {
            asrSetState(FOUNDOK);
            asrResult = (int)LD_ReadReg(0xc5);
            Serial.print("found ");
            Serial.println(asrResult);
            if(asrTable[asrResult][0][0]) {
                if(asrTable[asrResult][1][0]) {
                    sdPlay(asrTable[asrResult][1],1);
                    if(LD_Confirm())
                        asrExec(asrTable[asrResult][0]);
                    else
                        asrSetState(NONE);
                }else
                    asrExec(asrTable[asrResult][0]);
            }else if(asrDefault[0])
                asrExec(asrDefaultDir,asrDefault);
        }else {
            if(ret && ret <= 4) {
                Serial.print("asr result ");
                Serial.print((int)ret);
                Serial.print(',');
                asrResult = (int)LD_ReadReg(0xc5);
                Serial.print(asrResult);
                if(ret>1) {
                    Serial.print(',');
                    Serial.print((int)LD_ReadReg(0xc7));
                }
                if(ret>2) {
                    Serial.print(',');
                    Serial.print((int)LD_ReadReg(0xc9));
                }
                if(ret>3) {
                    Serial.print(',');
                    Serial.print((int)LD_ReadReg(0xcb));
                }
                Serial.println(' ');
                if(asrDefault[0]){
                    asrSetState(FOUNDZERO);
                    asrExec(asrDefaultDir,asrDefault);
                }else 
                    asrSetState(NONE);
            }else {
                Serial.println("no result");
                asrSetState(NONE);
            }
        }
        break;
    }
}
