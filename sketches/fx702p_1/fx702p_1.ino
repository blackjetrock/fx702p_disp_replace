/*
  LiquidCrystal Library - Hello World

  Demonstrates the use a 16x2 LCD display.  The LiquidCrystal
  library works with all LCD displays that are compatible with the
  Hitachi HD44780 driver. There are many of them out there, and you
  can usually tell them by the 16-pin interface.

  This sketch prints "Hello World!" to the LCD
  and shows the time.

  The circuit:
  * LCD RS pin to digital pin 12
  * LCD Enable pin to digital pin 11
  * LCD D4 pin to digital pin 5
  * LCD D5 pin to digital pin 4
  * LCD D6 pin to digital pin 3
  * LCD D7 pin to digital pin 2
  * LCD R/W pin to ground
  * LCD VSS pin to ground
  * LCD VCC pin to 5V
  * 10K resistor:
  * ends to +5V and ground
  * wiper to LCD VO pin (pin 3)

  Library originally added 18 Apr 2008
  by David A. Mellis
  library modified 5 Jul 2009
  by Limor Fried (http://www.ladyada.net)
  example added 9 Jul 2009
  by Tom Igoe
  modified 22 Nov 2010
  by Tom Igoe
  modified 7 Nov 2016
  by Arturo Guadalupi

  This example code is in the public domain.

  http://www.arduino.cc/en/Tutorial/LiquidCrystalHelloWorld

*/

// include the library code:
#include <LiquidCrystal.h>

// initialize the library by associating any needed LCD interface pin
// with the arduino pin number it is connected to
const int rs = PA4, en = PA5, d4 = PA0, d5 = PA1, d6 = PA2, d7 = PA3;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// Pin names
const int fxD0  = PB4;
const int fxD1  = PB5;
const int fxD2  = PB6;
const int fxD3  = PB7;
const int fxA0  = PB8;
const int fxA1  = PB9;
const int fxA2  = PB10;
const int fxA3  = PB11;
const int fxCE  = PB0;
const int fxOE  = PB1;
const int fxOP  = PB12;
const int fxWE  = PB13;
const int fxCLK = PB14;

// Pin bit offsets
const int fxnD0  = 4;
const int fxnD1  = 5;
const int fxnD2  = 6;
const int fxnD3  = 7;
const int fxnA0  = 8;
const int fxnA1  = 9;
const int fxnA2  = 10;
const int fxnA3  = 11;
const int fxnCE  = 0;
const int fxnOE  = 1;
const int fxnOP  = 12;
const int fxnWE  = 13;
const int fxnCLK = 14;

// Display buffer for the character display
// Interrupt routine populates buffer with fx702p character codes
// We need to translate to ASCII here, before display
#define DISPLAY_BUFFER_LEN 20

volatile int display_buffer[DISPLAY_BUFFER_LEN];

// Similar buffer for the four digit seven segment display. We map this to
// ASCII as well before display
volatile int seven_seg[4];

// Annunciator display bits. Non zero means annunciator is on
volatile int annunciators[12];

typedef struct
{
  int in;
  char ascii;
} MAP702P;

int map702p[] = {
  0x0f, ' ',
  0x11, '?',
  0x14, '\\',
  0x18, ':',
  0x30, '0',
  0x31, '1',
  0x32, '2',
  0x33, '3',
  0x34, '4',
  0x35, '5',
  0x36, '6',
  0x37, '7',
  0x38, '8',
  0x39, '9',
  0x40, 'A',
  0x41, 'B',
  0x42, 'C',
  0x43, 'D',
  0x44, 'E',
  0x45, 'F',
  0x46, 'G',
  0x47, 'H',
  0x48, 'I',
  0x49, 'J',
  0x4A, 'K',
  0x4B, 'L',
  0x4C, 'M',
  0x4D, 'N',
  0x4E, 'O',
  0x4F, 'P',
  0x50, 'Q',
  0x51, 'R',
  0x52, 'S',
  0x53, 'T',
  0x54, 'U',
  0x55, 'V',
  0x56, 'W',
  0x57, 'X',
  0x58, 'Y',
  0x59, 'Z',
  0x5E, '_',
  0x00, '@',   
};

char fx702pchar(int n)
{
  int i;
  
  for(i=0; map702p[i]!= 0; i+=2)
    {
      if ( n == map702p[i] )
	{
	  return(map702p[i+1]);
	}
      
    }
  return '?';
}


volatile int flag = 0;

void isr(void)
{
  // CE asserted, now clock data in, storing data and address
  flag = !flag;

  if ( flag )
    {
      display_buffer[0] = 0x51;
    }
  else
    {
      display_buffer[0] = 0x50;
    }
  
  
}

// CE asserted, get data
// We stay in this ISR until CE is de-asserted, it has priority over the
// main LCD display loop, as that loop won't have anything up to date until the ISR
// has run anyway.
// We could run an FSM instead

#define BITVAL(VAL,BITNUM) ((VAL & (1L << BITNUM)) >> BITNUM)
#define BITFIELD(VAL,LSBITNUM,MASK) ((VAL >> LSBITNUM) & MASK)

void ce_isr(void)
{
  int portval;
  int oe, ce, we, op, clk;
  int data, addr;
  
  // Read port B, that has all our signals
  
  portval = (PIN_MAP[fxCE].gpio_device)->regs->IDR;

  ce = BITVAL(portval, fxnCE);
  op = BITVAL(portval, fxnOP);
  we = BITVAL(portval, fxnWE);
  oe = BITVAL(portval, fxnOE);
  clk = BITVAL(portval, fxnCLK);
  data = BITFIELD(portval,fxnD0, 0xF);
  addr = BITFIELD(portval,fxnA0, 0xF);
  
  // display port
  display_buffer[0] = 0x30 + ce;
  display_buffer[1] = 0x30 + oe;
  display_buffer[2] = 0x30 + we;
  display_buffer[3] = 0x30 + op;
  display_buffer[4] = 0x30 + clk;
  
  display_buffer[9] = 0x30 + (portval & (1 << 13)) >> 13;

  Serial.println(portval);
}

void setup() {
  int i;

  Serial.begin(9600);
  
  // set up the LCD's number of columns and rows:
  lcd.begin(20, 4);
  
  // Print a message to the LCD.
  lcd.setCursor(0, 3);	
  lcd.print("READY P0:_123456789");

  for(i=0; i<DISPLAY_BUFFER_LEN; i++)
    {
      display_buffer[i] = 0x0F;
    }
  
  pinMode(PB0, INPUT);
  
  // Attach an interrupt to the CE line
  //  attachInterrupt(digitalPinToInterrupt(PB0), ce_isr, FALLING);
  attachInterrupt(PB0, ce_isr, FALLING);
  
}

volatile int f = 0;
volatile int i;

void loop() {
  //return;

  f = !f;
  
  // We just display the buffers contiuously
  // Interrupt routine might update mid loop, but next loop should
  // correct any incoherency.

  lcd.setCursor(0, 0);
  for(i=0; i< DISPLAY_BUFFER_LEN; i++)
    {
      lcd.setCursor(i, 0);
      lcd.print(fx702pchar(display_buffer[i]));
    }
  
  //return;
  // set the cursor to column 0, line 1
  // (note: line 1 is the second row, since counting begins with 0):
  lcd.setCursor(0, 1);
  // print the number of seconds since reset:

  if ( f )
    {
    lcd.print("F1 F2 RUN DEG TRACE");
  }
  else
    {
    lcd.print("   F2 RUN DEG TRACE");
    }

  if( digitalRead(PB0) )
    {
      lcd.setCursor(0, 2);
      lcd.print("HYP ARC PRINT");
    }
  else
    {
      lcd.setCursor(0, 2);
      lcd.print("    ARC PRINT");
    }
  

  // print the number of seconds since reset:
  //lcd.print(millis() / 100);

  //  Serial.println((PIN_MAP[fxCE].gpio_device)->regs->IDR);
    delay(1000);
}

