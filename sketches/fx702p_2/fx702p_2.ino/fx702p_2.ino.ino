/*
 * Replacement display for Casio fx702p
 *
 * Attach signals to bus and uses a blue pill PCB to display the fx702p
 * display output on a 20x4 LCD display.
 *
 *
 * 
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
const int fxCE2 = PB15;

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
const int fxnCE2 = 15;

// Display buffer for the character display
// Interrupt routine populates buffer with fx702p character codes
// We need to translate to ASCII here, before display
#define DISPLAY_BUFFER_LEN 20

volatile unsigned char display_buffer[DISPLAY_BUFFER_LEN];

// Similar buffer for the four digit seven segment display. We map this to
// ASCII as well before display
volatile int seven_seg[4];

// Annunciator display bits. Non zero means annunciator is on
volatile int annunciators[12];

char * anntext[12] = {
  "1",
  "2",
  "3",
  "4",
  "p",
  "6",
  "t",
  "8",
  "g",
  "r",
  "d",
  "c",
};

typedef struct
{
  int in;
  char ascii;
} MAP702P;

// It's difficult to map some characters, this table uses the A02 character set
// to have a go at some special characters
// like <> >= and <=
// Exponent E is also a problem.
//

unsigned char map702ps[259] =
// 0123456789ABCDEF
  "??????????????? "
  "????\"#$;:,>\xa6=\xd1<\xb7"
  "+-*/^?!?)???(???"
  "0123456789.\xf7???\xe3"
  "ABCDEFGHIJKLMNOP"
  "QRSTUVWXYZ????\x5f?"
  "????????????????"
  "????????????????"
  "????????????????"
  "????????????????"
  "????????????????"
  "????????????????"
  "????????????????"
  "????????????????"
  "????????????????"
  "????????????????"
  ;


char fx702pschar(unsigned char n)
{
  return(map702ps[n & 255]);
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

volatile  int dri = 0;
volatile  int reg[16];
#define REG_BUFFER_LEN 16

void ce_isr(void)
{
  int portval;
  int oe, ce, we, op, clk;
  unsigned char data, addr;
  int clock_phase = 1;  // First transition is phase 0
  int last_clk = 0;
  int done = 0;
  int active_edge = 0;
  int a;
  int glitch_count = 0;
  
  portval = (PIN_MAP[fxCE].gpio_device)->regs->IDR;
  last_clk = BITVAL(portval, fxnCLK);
  
  // Clock in on falling edges of clock, only use second clock
  while( !done )
    {
      // Read port B, that has all our signals
      portval = (PIN_MAP[fxCE].gpio_device)->regs->IDR;
      
      ce = BITVAL(portval, fxnCE);
      op = BITVAL(portval, fxnOP);
      we = BITVAL(portval, fxnWE);
      oe = BITVAL(portval, fxnOE);
      clk = BITVAL(portval, fxnCLK);
      
      // display port
#if 0      
      display_buffer[0] = 0x30 + ce;
      display_buffer[1] = 0x30 + oe;
      display_buffer[2] = 0x30 + we;
      display_buffer[3] = 0x30 + op;
      display_buffer[4] = 0x30 + clk;
      display_buffer[5] = 0x30 + data;
      display_buffer[6] = 0x30 + addr;

      display_buffer[addr] = 0x30+data;
    
#endif
      
      // Detect rising edges
      // Debounce possible glitches
      if( glitch_count > 0 )
	{
	  glitch_count--;
	}

      // Don't look for rising edges immediately after we have found one
      if( glitch_count == 0 )
	{
	  if( (last_clk == 0) && (clk == 1) )
	    {
	      active_edge = 1;
	      glitch_count = 2;
	      
	      // Next clock phase. First phase is 0, second phase is 1
	      // We ignore anything on phase 0
	      //
	      clock_phase = !clock_phase;
	    }
	  else
	    {
	      active_edge = 0;
	    }
	}
      last_clk = clk;
      
      if( (oe == 1) && active_edge && (clock_phase == 1) )
	{
	  data = BITFIELD(portval,fxnD0, 0xF);
	  addr = BITFIELD(portval,fxnA0, 0xF);
	  data ^= 0xF;
	  addr ^= 0xF;

	  // write or read cycle
	  
	  if( op == 0 )
	    {
	      int j;
	      
	      // Command read/write. reads and writes seem to set commands up
	      for(j=0;j<16;j++)
		{
		  reg[j] = 0;
		}
	      
	      reg[addr] = data;

#if 0
	      display_buffer[dri++] = data+0x30;
	      if( dri >= 20 )
		{
		  dri = 0;
		}
#endif
	    }
	}

      
      // Data write
      if( (op == 1) && (we == 0) && active_edge && (clock_phase ==1) )
	{
	  data = BITFIELD(portval,fxnD0, 0xF);
	  addr = BITFIELD(portval,fxnA0, 0xF);
	  data ^= 0xF;
	  addr ^= 0xF;
	  
	  //display_buffer[1] = 0x43;
#if 0
	  display_buffer[dri++] = data+0x30;
	  if( dri >= 20 )
	    {
	      dri = 0;
	    }
#endif
	  if( reg[0] = 0xB )
	    {
	      if( (addr >=0) && (addr <=3) )
		{
		  seven_seg[addr] = data;
		}
	    }

	  if( reg[0xB] = 0x4 )
	    {
	      if( (addr >=1) && (addr <=0xB) )
		{
		  annunciators[addr] = data;
		}
	    }

	  
	  if( (addr>=3) && (addr<=12))
	    {
	      switch(reg[3])
		{
		case 0xC:
		  a = (addr - 3)+10;
		  if( (a >= 0) && (a <=19))
		    {
		      display_buffer[a] = (display_buffer[a] & 0x0F) + (data << 4);
		    }
		  break;
		  
		case 0xD:
		  a = (addr - 3)+10;
		  if( (a >= 0) && (a <=19))
		    {
		      display_buffer[a] = (display_buffer[a] & 0xF0) + (data << 0);
		    }
		  break;
		  
		case 0xE:
		  a = (addr - 3);
		  if( (a >= 0) && (a <=19))
		    {
		      display_buffer[a] = (display_buffer[a] & 0x0F) + (data << 4);
		    }
		  break;
		  
		case 0xF:
		  a = (addr - 3);
		  if( (a >= 0) && (a <=19))
		    {
		      display_buffer[a] = (display_buffer[a] & 0xF0) + (data << 0);
		    }
		  break;
		  
		default:
		  //	  display_buffer[addr] = reg[3]+0x30;
		  break;
		}
	    }
	}
      
      
      // CE goes inactive, we stop processing the cycle
      if( ce == 1 )
	{
	  done = 1;
	}
    }  
}


void setup() {
  int i;

  for(i=0;i<4;i++)
    {
      seven_seg[i] = 0;
    }
  
  Serial.begin(9600);
      
  // set up the LCD's number of columns and rows:
  lcd.begin(20, 4);
  
  for(i=0; i<DISPLAY_BUFFER_LEN; i++)
    {
      display_buffer[i] = 0x0F;
    }
  
  pinMode(PB0, INPUT);
  pinMode(PB15, INPUT);

  // Attach an interrupt to the CE line
  //  attachInterrupt(digitalPinToInterrupt(PB0), ce_isr, FALLING);
  attachInterrupt(PB0, ce_isr, FALLING);
  //  attachInterrupt(PB15, ce_isr, FALLING);
  
}

volatile int f = 0;
volatile int i;

void loop() {
  int j;
  
  //return;

  f = !f;
  
  // We just display the buffers contiuously
  // Interrupt routine might update mid loop, but next loop should
  // correct any incoherency.

  lcd.setCursor(0, 0);
  for(i=0; i< DISPLAY_BUFFER_LEN; i++)
    {
      lcd.setCursor(19-i, 0);
      lcd.print(fx702pschar(display_buffer[i]));
    }

#if 0
  lcd.setCursor(0, 3);
  for(i=0; i< REG_BUFFER_LEN; i++)
    {
      lcd.setCursor(i, 3);
      lcd.print(fx702pschar(reg[i]+0x30));
    }
#endif
  //return;
  // set the cursor to column 0, line 1
  // (note: line 1 is the second row, since counting begins with 0):
  lcd.setCursor(0, 1);
  // print the number of seconds since reset:

  if ( f )
    {
      lcd.setCursor(19, 3);
      lcd.print("*");
    }
  else
    {
      lcd.setCursor(19, 3);
      lcd.print(" ");
    }

  lcd.setCursor(0,3);
  
  int v = 0;
  for(j=3;j>=0;j--)
    {
      v*= 10;
      v+= seven_seg[j];
    }
  lcd.print(v);
  lcd.print(".");
	  
  lcd.setCursor(0, 2);
  Serial.print(j);
  Serial.print(":");
  
  for(j=1;j<0xB;j++)
    {
      Serial.print(annunciators[j]);
      Serial.print(" ");
      
      if( annunciators[j] )
	{
	  lcd.print(annunciators[j]);
	}
      else
	{
	  lcd.print(" ");  
	}
    }
  Serial.println("");
  
  lcd.setCursor(0,1);
  if( annunciators[8] )
    {
      lcd.print("GRA");
    }
  if( annunciators[9] )
    {
      lcd.print("RAD");
    }
  if( annunciators[10] )
    {
      lcd.print("DEG");
    }

  lcd.setCursor(4,1);
  if( annunciators[6] )
    {
      lcd.print("TRC");
    }
  else
    {
      lcd.print("   ");
    }

  lcd.setCursor(8,1);
  if( annunciators[4] )
    {
      lcd.print("PRT");
    }
  else
    {
      lcd.print("   ");
    }

  // print the number of seconds since reset:
  //lcd.print(millis() / 100);

  //  Serial.println((PIN_MAP[fxCE].gpio_device)->regs->IDR);
  delay(1);

#if 0
  for(i=0; i<=19;i++)
    {
      Serial.print(fx702pschar(display_buffer[i]));
      Serial.print(" ");  
    }
  Serial.println("");  

  for(i=0; i<=19;i++)
    {
      Serial.print(display_buffer[i], HEX);
      Serial.print(" ");  
    }
  Serial.println("");
#endif
}

