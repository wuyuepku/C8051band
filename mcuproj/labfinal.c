#include <c8051f020.h>
#include <stdio.h>
#include "tftlib.h"

#define DELAY_LCD 100
#define SYSCLK 22118400
#define BAUDRATE 460800
#define AUDIORATE 8000

sfr16 RCAP4 = 0xE4;
sfr16 T4 = 0xF4;
sfr16 DAC0 = 0xD2;
sfr16 DAC1 = 0xD5;

sfr16 TMR3RL = 0x92;
sfr16 TMR3 = 0x94;
sfr16 ADC0 = 0xbe;

void main(void);
void SYSCLK_Init(void);
void PORT_Init(void);
void UART0_Init(void);
void Delay(unsigned int k);
void Timer4_Init(int counts);
char putchar(char c);

void Lcd1602_init(void);
void Lcd1602_Write_Command(unsigned char Command);
void Lcd1602_Write_Data(unsigned char Data);

unsigned char Check_Keyboard(void);
#define NOKEY 255
unsigned char lastkey = NOKEY;
unsigned char Get_Newkey(void);  // will return NOKEY if the same with last one

#define MODE_WAIT_CONNECTION 0
#define MODE_CONNECTED 1
unsigned char _mode = 0xFF;
unsigned char mode = MODE_WAIT_CONNECTION;
void show_mode(void);
void ____putchar(char c, unsigned char threshold);

unsigned int audio_read = 0;
unsigned int audio_write = 0;
#define audio_mask 0x03FF
unsigned int xdata audio_buf[1024];  // 2048 byte
#define audio_empty() (audio_read == audio_write)
#define audio_get(var) var = audio_buf[audio_read]; audio_read = (audio_read+1) & audio_mask
#define audio_full() ( ((audio_write+1) & audio_mask) == audio_read )
#define audio_put(var) audio_buf[audio_write] = var; audio_write = (audio_write+1) & audio_mask
#define audio_length() ( (audio_write + 0x400 - audio_read) & audio_mask )

void main(void) {
	int cnt = 0;

	WDTCN = 0xDE;
	WDTCN = 0xAD;
	SYSCLK_Init();
	PORT_Init();
	UART0_Init();
	Lcd1602_init();
	lcd_init9486();
	REF0CN = 0x03;
	DAC1CN = 0x90;  // right aligned, using timer4
	ES0 = 1;
	TI0 = 1;
	EA = 1;
	Timer4_Init(SYSCLK/AUDIORATE);

	printf("C8051band v0.0.1\n");
	printf("compiled at %s, %s\n", __TIME__, __DATE__);
	Lcd1602_Write_Command(0x80);
	display_color(BKGCOLOR);

	log_init();
	log_push("hello world!");
	log_push("yes!");

	while (1) {
		char buf[10];
		unsigned char a = Get_Newkey();
		sprintf(buf, "line: %d", cnt);
		++cnt;
		log_push(buf);
		if (a != NOKEY) {
			printf("p%c\n", a<10?'0'+a:'A'+(a-10));
		}
		show_mode();
	}

}

char xdata charfifo[256];
unsigned char fiforead = 0;
unsigned char fifowrite = 0;
#define fifocount() (fifowrite - fiforead)
#define fifofull() (fifowrite + 1 == fiforead)
#define fifoempty() (fifowrite == fiforead)
#define fifolock() (EA = 0)
#define fifounlock() (EA = 1)
#define fifoenque(c) do { charfifo[fifowrite] = c; ++fifowrite; } while(0)
#define fifodeque(c) do { c = charfifo[fiforead]; ++fiforead; } while(0)
void Timer4_ISR(void) interrupt 16 {
	if (mode == MODE_WAIT_CONNECTION) {
		if (audio_length() > 768) mode = MODE_CONNECTED;
	} else if (mode == MODE_CONNECTED) {
		if (audio_empty()) {
			mode = MODE_WAIT_CONNECTION;
		} else {
			audio_get(DAC1);  // update DAC value
		}
	}
	____putchar(0x8F, 250);  // request a sample, highest priority
	T4CON &= ~0x80;  // clear overflow flag
	// static unsigned phase = 0;
	// DAC0 = phase;
	// phase += 0x10;
	// T4CON &= ~0x80;
}

volatile char putchar_stopped = 1;
void ____putchar(char c, unsigned char threshold) {
	while (fifocount() > threshold);  // half full
	fifolock();
	if (fifofull()) while(1);
	fifoenque(c);
	if (putchar_stopped) {
		TI0 = 1;
		putchar_stopped = 0;
	}
	fifounlock();
}
char putchar(char c)  {
	____putchar(c, 128);
	return c;
}

void UART0_ISR(void) interrupt 4 {
	char c;
	static unsigned char audio_tmp;
	if (RI0 == 1) {  // rx interrupt
		c = SBUF0;
		RI0 = 0;
		if (c & 0x80) {  // audio data
			if (c & 0x40) {
				if (audio_full()) while(1);  // need to debug >.<
				audio_put(((unsigned int)audio_tmp << 6 | (c & 0x3F)));
			} else {
				audio_tmp = c & 0x3F;
			}
		} else {
			
		}
	}
	if (TI0 == 1) {
		if (fifoempty()) {
			putchar_stopped = 1;
			TI0 = 0;  // someone may restart this, that's OK
		} else {
			TI0 = 0;
			fifodeque(SBUF0);
		}
	}
}

char xdata outbuf[64];
char xdata inbuf[16];
unsigned char inidx = 0;
char c2char(char a) {
	if (a >= 0 && a <= 9) return 0x30 | a;
	if (a == 0x0A) return 0x2B;
	if (a == 0x0B) return 0x2D;
	if (a == 0x0C) return 0x78;
	if (a == 0x0D) return 0xFD;
	if (a >= '0' && a <= '9') return 0x30 + a - '0';
	if (a >= 'a' && a <= 'z') return 0x61 + a - 'a';
	if (a >= 'A' && a <= 'Z') return 0x41 + a - 'A';
	if (a == ' ') return 0x20;
	if (a == '.') return 0x2E;
	if (a == ':') return 0x3A;
	if (a == '+') return 0x2B;
	if (a == '-') return 0x2D;
	return 0x00;
}
void write_string(const char* str) {
	unsigned char i;
	for (i=0; str[i] && i<16; ++i) {
		Lcd1602_Write_Data(c2char(str[i]));
	} for (; i<16; ++i) Lcd1602_Write_Data(0x20);
}

void show_mode() {
	if (_mode != mode) {
		_mode = mode;
		if (mode == MODE_WAIT_CONNECTION) {
			Lcd1602_Write_Command(0xC0);  // second line
			write_string("wait server ...");
			Lcd1602_Write_Command(0x80 + inidx);  // return cursor
		} else if (mode == MODE_CONNECTED) {
			Lcd1602_Write_Command(0xC0);  // second line
			write_string("server connected");
			Lcd1602_Write_Command(0x80 + inidx);  // return cursor
		}
	}
}

void SYSCLK_Init(void) {
	int i;
	OSCXCN = 0x67;
	for (i=0; i<256; ++i);
	while (!(OSCXCN & 0x80));
	OSCICN = 0x88;
}

void Delay(unsigned int k) {
	unsigned int i;
	for (i=0; i<k; ++i);
}

void PORT_Init(void) {
	EMI0CF = 0x1F; // non-multiplexed mode, external only
	XBR0 = 0x04;  // enable UART0
	XBR2 = 0x42;  // 01000010, XBARE=1(enable XBR), EMIFLE=1(P0.7 P0.6 as WR RD)
	P74OUT = 0x30;  // P6 push-pull
	P0MDOUT = 0xC1;
	P1MDOUT = 0xFF;  // push pull
	P2MDOUT = 0xFF;
	P3MDOUT = 0xFF;
	EMI0TC = 0x41;  // external memory timing control: 1 sysclk cycle setup and hold time.
}

void UART0_Init(void) {
	SCON0 = 0x50;  // enable rx
	TMOD = 0x20;  // timer1 autoreload
	CKCON |= 0x10;  // no divide 12 !!! for higher baudrate
	TH1 = -(SYSCLK/BAUDRATE/16);  // reload
	TR1 = 1;  // start timer1
	PCON |= 0x80;  // double baud rate
}

void Timer4_Init(int counts) {
	T4CON = 0;
	CKCON |= 0x40;
	RCAP4 = -counts;
	T4 = RCAP4;
	EIE2 |= 0x04;
	T4CON |= 0x04;
}

char isLcdBusy(void) {
	P5 = 0xFF;
	P6 = 0x82;
	Delay(DELAY_LCD);
	P6 = 0x83;
	Delay(DELAY_LCD);
	return (P5 & 0x80);
}

void Lcd1602_Write_Command(unsigned char Command) {
	while(isLcdBusy());
	P5 = Command;
	P6 = 0x80;
	Delay(DELAY_LCD);
	P6 = 0x81;
	Delay(DELAY_LCD);
	P6 = 0x80;
}

void Lcd1602_Write_Data(unsigned char Data) {
	P5 = Data;
	P6 = 0x84;
	Delay(DELAY_LCD);
	P6 = 0x85;
	Delay(DELAY_LCD);
	P6 = 0x84;
}

void Lcd1602_init(void) {
	Lcd1602_Write_Command(0x38);
	Lcd1602_Write_Command(0x08);
	Lcd1602_Write_Command(0x01);
	Lcd1602_Write_Command(0x06);
	Lcd1602_Write_Command(0x0C);
	Lcd1602_Write_Command(0x80);
	Lcd1602_Write_Command(0x02);
}

unsigned char Check_Keyboard(void) {
	unsigned char i;
	unsigned char key;
	const unsigned code dec[] = {0,0,1,0,2,0,0,0,3,0,0,0,0,0,0,0};
	const unsigned code trans[] = {0xC,9,5,1,0xD,0,6,2,0xE,0xA,7,3,0xF,0xB,8,4};
	P4 = 0x0F;
	Delay(100);
	i = ~P4 & 0x0F;
	if (i == 0) return NOKEY;
	key = dec[i] * 4;
	Delay(1000);
	P4 = 0xF0;
	Delay(100);
	i = ~P4;
	i >>= 4;
	if (i == 0) return NOKEY;
	key = key + dec[i];
	key = trans[key];
	return key;
}

unsigned char Get_Newkey(void) {
	unsigned k = Check_Keyboard();
	if (k != lastkey) {
		lastkey = k;
		return lastkey;
	}
	return NOKEY;
}
