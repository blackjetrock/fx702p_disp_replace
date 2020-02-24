/*
 * Replacement display for Casio fx702p
 *
 * Attach signals to bus and uses a blue pill PCB to display the fx702p
 * display output on a 20x4 LCD display.
 *
 *
 * 
 */

// The LCD library is used to display the data we collect
#include <LiquidCrystal.h>

// Serial output only works in logic analyser mode.
#define SERIAL_DUMP_REGS 0
#define SERIAL_TAGS      0
#define SERIAL_7SEG      0
#define SERIAL_CE3       1
#define SERIAL_ANN       0
#define SERIAL_SIGNALS   0
#define SERIAL_REGDUMP   0

#define FLAG_7SEG_COLON  0
#define LCD_ANNUNCIATORS  0

// There's two ways to get the data:
//
// A.Logic analyser
//   Capture all signals in ISr and process those 'logic analyser' traces
//   in the main loop. This is slower but all signals can be displayed over the
//   serial port for analysis.Some display is missed in this mode.
//
// B. ISR processing
//   The ISR captures bus cycles and builds arrays of data that are displayed by the
//   main loop. Much faster and more efficient.
//

#define LOGIC_ANALYSER  1

#define INLINE 0

// initialize the library by associating any needed LCD interface pin
// with the arduino pin number it is connected to
#define  PCB_VERSION 1

#if !PCB_VERSION
// Wired version

const int rs = PA4, en = PA5, d4 = PA0, d5 = PA1, d6 = PA2, d7 = PA3;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);
o
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

#else
// PCB version
const int rs = PA4, en = PA5, d4 = PA3, d5 = PA2, d6 = PA1, d7 = PA0;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// Pin names
const int fxD0  = PB12;
const int fxD1  = PB13;
const int fxD2  = PB14;
const int fxD3  = PB15;
const int fxA0  = PB6;
const int fxA1  = PB7;
const int fxA2  = PB8;
const int fxA3  = PB9;
const int fxCE  = PA15;
const int fxOE  = PB3;
const int fxOP  = PB4;
const int fxWE  = PB11;
const int fxCLK = PB10;
const int fxCE2 = PA12;
const int fxCE3 = PA8;

// Pin bit offsets
const int fxnD0  = 12;
const int fxnD1  = 13;
const int fxnD2  = 14;
const int fxnD3  = 15;
const int fxnA0  = 6;
const int fxnA1  = 7;
const int fxnA2  = 8;
const int fxnA3  = 9;
const int fxnCE  = 15;    // Port A
const int fxnOE  = 3;
const int fxnOP  = 4;
const int fxnWE  = 11;
const int fxnCLK = 10;
const int fxnCE3 = 8;     // Port A

#endif

// Display buffer for the character display
// Interrupt routine populates buffer with fx702p character codes
// We need to translate to ASCII here, before display
#define DISPLAY_BUFFER_LEN 20

volatile unsigned char display_buffer[DISPLAY_BUFFER_LEN];

// Similar buffer for the four digit seven segment display. We map this to
// ASCII as well before display
volatile int seven_seg[4];

// Annunciator display bits. Non zero means annunciator is on
// 1-C are annunciators, 0 unused
// D and E are 9AB 000 addresses 1 and 2

volatile int annunciators[20];

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

volatile int ab_index = 0;
volatile int ce3_reg_ab_data[16];
volatile int ce3_reg_ab2_data[16];
volatile int ce3_reg_45_data[16];
volatile int ce3_reg_23_data[16];
volatile int ce3_reg_value[16];
unsigned char reg[17];
boolean ce3_23_reg = false;
boolean ce3_ab_reg = false;
boolean ce3_45_reg = false;

#define REG_BUFFER_LEN 16

void proc_seven_seg(unsigned char reg[], unsigned char addr, unsigned char data)
{
  int i;
  
  if( (reg[0] == 0xB) && (reg[3]==0xE) && (reg[4]==0) && (reg[5]==8) )
    {
#if SERIAL_TAGS
      Serial.println("7SEG");
#endif
#if SERIAL_7SEG
      Serial.print("7seg");
#endif
      if( (addr >=0) && (addr <=3) )
	{
	  seven_seg[addr] = data;
#if SERIAL_7SEG
	  
	  Serial.print(data, HEX);
#endif
	  
	}
#if SERIAL_7_SEG
      Serial.println("");
#endif
      
      // Reset reg[0] once done
      if( (addr == 3) )
	{
	  reg[0] = 100;
	  reg[3] = 100;
	}
    }
}

void proc_cursor(unsigned char reg[], unsigned char addr, unsigned char data)
{
  unsigned char a;
  int i;
  
#if 1
  if( (reg[1] == 0) && (reg[2] == 0))
    {
      if( (addr>=3) && (addr<=12))
	{
#if SERIAL_TAGS
	  Serial.println("CURUPD");
#endif
	  for(i=0;i<16;i++)
	    {
	      reg[i] = 100;
	    }
	  switch(0xF)
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

  if( ((reg[5] == 9) && (reg[6] == 4))   )
    {
      if( (addr>=3) && (addr<=12))
	{
#if SERIAL_TAGS
	  Serial.println("CURUPD");
#endif
	  for(i=0;i<16;i++)
	    {
	      reg[i] = 100;
	    }

	  switch(0xE)
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
		  // For some reason top bit sometimes set, clear it
		  display_buffer[a] = (display_buffer[a] & 0x0F) + ((data & 7) << 4);
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
#endif

}

void proc_annunciators(unsigned char reg[], unsigned char addr, unsigned char data)
{
  if( (reg[9] == 0) && (reg[0xA]==0) && (reg[0xB]==4)  )
    {
#if SERIAL_TAGS	      
      Serial.print("9AB ");
      Serial.print(addr,HEX);
      Serial.print(" ");
      Serial.println(data,HEX);
#endif
      // Annunciators
      if( (addr >=1) && (addr <=12) )
	{
	  annunciators[addr] = (data & 0x8);
	}
    }

  if( (reg[9] == 0) && (reg[0xA]==0) && (reg[0xB]==0)  )
    {
#if SERIAL_TAGS
      Serial.print("9AB 000");
      Serial.print(addr,HEX);
      Serial.print(" ");
      Serial.println(data,HEX);
#endif
      // Annunciators
      if( (addr >=1) && (addr <=2) )
	{
	  annunciators[addr-1+13] = (data);
	}
    }
}

void proc_matrix(unsigned char reg[], unsigned char addr, unsigned char data)
{
  unsigned char a;
  int i;
  
  if( ((reg[4] == 0) && (reg[5] == 8) && (reg[0] == 100))   )
    {
      if( (addr>=3) && (addr<=12))
	{
	  // Start of matrix update, clear any other commands
	  reg[0] = 100;
	  reg[1] = 100;
	  reg[2] = 100;
	  for(i=6;i<=0xC;i++)
	    {
	      reg[i] = 100;
	    }
#if SERIAL_TAGS		  
	  Serial.println("MATUPD");
#endif
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
}

void proc_regs(boolean op, boolean oe, boolean we, unsigned char data, unsigned char addr, unsigned char reg[])
{
  int i;
  
  // Get character data
  if( op == false )
    {
      switch(addr)
	{
	  // Start of new commands
	case 2:
	  if( reg[1] != 100 )
	    {
	      reg[0] = 100;
	      for(i=3;i<16;i++)
		{
		  reg[i] = 100;
		}
	    }
	  break;
	  
	case 6:
	  if( reg[5] != 100 )
	    {
	      
	      for(i=0;i<=4;i++)
		{
		  reg[i] = 100;
		}
	      for(i=7;i<16;i++)
		{
		  reg[i] = 100;
		}
	    }
	  break;
	  
	  // Can't reset commands with reg3 as sometimes it is issued on it's own
	case 3:
	  break;
	  
	case 4:
	  // Don't wipe out 3
#if SERIAL_TAGS
	  Serial.println("CLRregn3");
#endif
	  for(i=0;i<=2;i++)
	    {
	      reg[i] = 100;
	    }
	  for(i=4;i<=15;i++)
	    {
	      reg[i] = 100;
	    }
	  break;
	  
	case 0xc:
	case 9:
	  // case 0:
	  // reg 0 reset means 7 seg a problem.
#if SERIAL_TAGS
	  Serial.println("CLRreg");
#endif
	  for(i=0;i<16;i++)
	    {
	      reg[i] = 100;
	    }
	  break;
	  
	default:
	  break;
	}
      // register write
      reg[addr] = data;
    }
}


//--------------------------------------------------------------------------------
//
// Processing ISR
//

#if !LOGIC_ANALYSER

void ce1_isr(void)
{
  volatile int portval, portvalce;
  boolean oe, ce, we, op, clk;

  unsigned char data, addr;
  int clock_phase = 0;  // First transition is phase 0
  int last_clk = 0;
  int done = 0;
  int active_edge = 0;
  int a;
  int i;
  int glitch_count = 0;

  portval = (PIN_MAP[fxD0].gpio_device)->regs->IDR;
  last_clk = BITVAL(portval, fxnCLK);
  
  // Clock in on falling edges of clock, only use second clock
  while( !done )
    {
      // Read port B, that has all our signals
      portval = (PIN_MAP[fxD0].gpio_device)->regs->IDR;
      portvalce = (PIN_MAP[fxCE].gpio_device)->regs->IDR;
	          
      ce = BITVAL(portvalce, fxnCE);
      op = BITVAL(portval, fxnOP);
      we = BITVAL(portval, fxnWE);
      oe = BITVAL(portval, fxnOE);
      clk = BITVAL(portval, fxnCLK);
      
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

      // register writes
      
      if( (oe == 1) && active_edge && (clock_phase == 1) )
	{
	  data = BITFIELD(portval,fxnD0, 0xF);
	  addr = BITFIELD(portval,fxnA0, 0xF);
	  data ^= 0xF;
	  addr ^= 0xF;

	  proc_regs(op, oe, we, data, addr, reg);
	}
      
      // Data write
      if( (op == 1) && (we == 0) && active_edge && (clock_phase ==1) )
	{
	  data = BITFIELD(portval,fxnD0, 0xF);
	  addr = BITFIELD(portval,fxnA0, 0xF);
	  data ^= 0xF;
	  addr ^= 0xF;

	  proc_seven_seg(reg, addr, data);
	  proc_matrix(reg, addr, data);
	  proc_annunciators(reg, addr, data);
	  proc_cursor(reg, addr, data);
	}
      
      
      // CE goes inactive, we stop processing the cycle
      if( ce == 1 )
	{
	  done = 1;
	}
    }  
}

// ISR for RH display chip

void ce3_isr(void)
{
  volatile int portval, portvalce;
  volatile int oe, ce, we, op, clk;
  unsigned char data, addr;
  int clock_phase = 0;  // First transition is phase 0
  int last_clk = 0;
  int done = 0;
  int active_edge = 0;
  int a;
  int glitch_count = 0;

  portval = (PIN_MAP[fxD0].gpio_device)->regs->IDR;
  last_clk = BITVAL(portval, fxnCLK);
  
  // Clock in on falling edges of clock, only use second clock
  while( !done )
    {
      // Read port B, that has all our signals
      portval = (PIN_MAP[fxD0].gpio_device)->regs->IDR;
      portvalce = (PIN_MAP[fxCE3].gpio_device)->regs->IDR;
      
      ce = BITVAL(portvalce, fxnCE3);
      op = BITVAL(portval, fxnOP);
      we = BITVAL(portval, fxnWE);
      oe = BITVAL(portval, fxnOE);
      clk = BITVAL(portval, fxnCLK);
      
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

#if 0
	  if( op == 0 )
	    {
	      int j;
	      
	      // Command read/write. reads and writes seem to set commands up
	      for(j=0;j<16;j++)
		{
		  reg[j] = 100;
		}
	      
	      reg[addr] = data;
	      
	    }
#endif
	}
      
      
      // Data write
      if( (op == 1) && active_edge && (clock_phase ==1) )
	{
	  data = BITFIELD(portval,fxnD0, 0xF);
	  addr = BITFIELD(portval,fxnA0, 0xF);
	  data ^= 0xF;
	  addr ^= 0xF;

#if 0
	  if( reg[0xB] = 0x4 )
	    {
	      if( (addr >=1) && (addr <=0xB) )
		{
		  annunciators[addr] = data;
		}
	    }
#endif
	}

      
      // CE goes inactive, we stop processing the cycle
      if( ce == 1 )
	{
	  done = 1;
	}
    }  
}

#endif

////////////////////////////////////////////////////////////////////////////////////////////////////

// ISR that captures a trace of bu activity which is then decoded by
// the main loop. This allows the code to be used as a sort of logic analyser

#define MASK_OP  1
#define MASK_OE  2
#define MASK_WE  4

#define BITNUM_OP  0
#define BITNUM_OE  1
#define BITNUM_WE  2

#define FLAGS 1

typedef struct
{
  unsigned char flags;
  //boolean op;          // Command or data
  //boolean we;
  //boolean oe;
  unsigned char addr;
  unsigned char data;
} ISR_TRACE_ENTRY;

volatile int isr_trace_1_in;  // Where next entry goes in
volatile int isr_trace_1_out; // Where next entry goes out
volatile boolean overflow_1 = false;    // If the buffer overflows

#define NUM_ISR_CE1_TRACE    2900
volatile ISR_TRACE_ENTRY isr_trace_ce1[NUM_ISR_CE1_TRACE];

volatile int isr_trace_3_in;  // Where next entry goes in
volatile int isr_trace_3_out; // Where next entry goes out
volatile boolean overflow_3 = false;    // If the buffer overflows

#define NUM_ISR_CE3_TRACE    1000
volatile ISR_TRACE_ENTRY isr_trace_ce3[NUM_ISR_CE3_TRACE];

// ISR for the CE interrupt
// We will probably need two ISRs, one for each display chip CE
// This ISR is for CE3 (LH display chip)

#if LOGIC_ANALYSER

void ce3_isr_2(void)
{
  volatile int portval, portvalce;
  volatile int oe, ce, we, op, clk;
  unsigned char data, addr;
  int clock_phase = 0;  // First transition is phase 0
  int last_clk = 0;
  int done = 0;
  int active_edge = 0;
  int a;

  portval = (PIN_MAP[fxD0].gpio_device)->regs->IDR;
  last_clk = BITVAL(portval, fxnCLK);
  
  // Clock in on falling edges of clock, only use second clock
  while( !done )
    {
      // Read port B, that has all our signals
      portval = (PIN_MAP[fxD0].gpio_device)->regs->IDR;
      //      portvalce = (PIN_MAP[fxCE].gpio_device)->regs->IDR;
      portvalce = (PIN_MAP[PA8].gpio_device)->regs->IDR;
      
      //      ce = BITVAL(portvalce, fxnCE);
      ce = BITVAL(portvalce, 8);
      op = BITVAL(portval, fxnOP);
      we = BITVAL(portval, fxnWE);
      oe = BITVAL(portval, fxnOE);
      clk = BITVAL(portval, fxnCLK);

      // Detect rising edges
      if( (last_clk == 0) && (clk == 1) )
	{
	  active_edge = 1;
	  
	  // Next clock phase. First phase is 0, second phase is 1
	  // We ignore anything on phase 0
	  //
	  clock_phase = !clock_phase;
	}
      else
	{
	  active_edge = 0;
	}
      last_clk = clk;

      // If it's a clock edge and an active (alternate) clock edge, capture data
      if( active_edge && (clock_phase == 0) )
	{
	  data = BITFIELD(portval,fxnD0, 0xF);
	  addr = BITFIELD(portval,fxnA0, 0xF);
	  data ^= 0xF;
	  addr ^= 0xF;
	  
	  // Fill out trace entry and we are done
	  if( ((isr_trace_3_in+1)%NUM_ISR_CE3_TRACE) == isr_trace_3_out )
	    {
	      // Buffer full
	      overflow_3 = true;
	      return;
	    }

#if FLAGS
	  isr_trace_ce3[isr_trace_3_in].flags = (op << BITNUM_OP) + (oe << BITNUM_OE) + (we << BITNUM_WE);;
#else

	  isr_trace_ce3[isr_trace_3_in].op = op;
	  isr_trace_ce3[isr_trace_3_in].oe = oe;
	  isr_trace_ce3[isr_trace_3_in].we = we;
#endif
	  isr_trace_ce3[isr_trace_3_in].data = data;
	  isr_trace_ce3[isr_trace_3_in].addr = addr;
	  
	  isr_trace_3_in = ((isr_trace_3_in+1) % NUM_ISR_CE3_TRACE);
	  
	}
      
      
      // CE goes inactive, we stop processing the cycle
      if( ce == 1 )
	{
	  done = 1;
	}
    }  
#if 0
  display_buffer[6] = 0x41;
#endif
  
}

// CE1 ISR, same as CE3, but different buffer etc. Two copies for more speed

void ce1_isr_2(void)
{
  volatile int portval, portvalce;
  volatile int oe, ce, we, op, clk;
  unsigned char data, addr;
  int clock_phase = 0;  // First transition is phase 0
  int last_clk = 0;
  int done = 0;
  int active_edge = 0;
  int a;
  int glitch_count = 0;

#if 0
  display_buffer[5] = 0x40;
#endif
  
  portval = (PIN_MAP[fxD0].gpio_device)->regs->IDR;
  last_clk = BITVAL(portval, fxnCLK);
  
  // Clock in on falling edges of clock, only use second clock
  while( !done )
    {
      // Read port B, that has all our signals
      portval = (PIN_MAP[fxD0].gpio_device)->regs->IDR;
      portvalce = (PIN_MAP[fxCE].gpio_device)->regs->IDR;
      //portvalce = (PIN_MAP[PA8].gpio_device)->regs->IDR;
      
      ce = BITVAL(portvalce, fxnCE);
      //      ce = BITVAL(portvalce, fxnCE);
      op = BITVAL(portval, fxnOP);
      we = BITVAL(portval, fxnWE);
      oe = BITVAL(portval, fxnOE);
      clk = BITVAL(portval, fxnCLK);

      // Detect rising edges
      if( (last_clk == 0) && (clk == 1) )
	{
	  active_edge = 1;
	  
	  // Next clock phase. First phase is 0, second phase is 1
	  // We ignore anything on phase 0
	  //
	  clock_phase = !clock_phase;
	}
      else
	{
	  active_edge = 0;
	}
      
      last_clk = clk;

      // If it's a clock edge and an active (alternate) clock edge, capture data
      if( active_edge && (clock_phase == 1) )
	{
	  data = BITFIELD(portval,fxnD0, 0xF);
	  addr = BITFIELD(portval,fxnA0, 0xF);
	  data ^= 0xF;
	  addr ^= 0xF;
	  
	  // Fill out trace entry and we are done
	  if( ((isr_trace_1_in+1)%NUM_ISR_CE1_TRACE) == isr_trace_1_out )
	    {
	      // Buffer full
	      overflow_1 = true;
	      return;
	    }

#if FLAGS
	  isr_trace_ce1[isr_trace_1_in].flags = (op << BITNUM_OP) + (oe << BITNUM_OE) + (we << BITNUM_WE);
#else

	  isr_trace_ce3[isr_trace_1_in].op = op;
	  isr_trace_ce3[isr_trace_1_in].oe = oe;
	  isr_trace_ce3[isr_trace_1_in].we = we;
#endif

	  isr_trace_ce1[isr_trace_1_in].data = data;
	  isr_trace_ce1[isr_trace_1_in].addr = addr;
	  
	  isr_trace_1_in = ((isr_trace_1_in+1)%NUM_ISR_CE1_TRACE);
	  
	}
      
      
      // CE goes inactive, we stop processing the cycle
      if( ce == 1 )
	{
	  done = 1;
	}
    }  
#if 0
  display_buffer[6] = 0x41;
#endif
  
}

#endif

////////////////////////////////////////////////////////////////////////////////////////////////////


void setup() {
  int i;

  // Initialise trace buffer
  isr_trace_1_in = 0;  // Where next entry goes in
  isr_trace_1_out = 0;  // Where next entry goes out
  isr_trace_3_in = 0;  // Where next entry goes in
  isr_trace_3_out = 0;  // Where next entry goes out
  
  for(i=0;i<16;i++)
    {
      reg[i] = 100;
    }
  
  for(i=0;i<4;i++)
    {
      seven_seg[i] = 0;
    }
  
  Serial.begin(2000000);
  Serial.println("fx702p Replacement Display");
  
  // set up the LCD's number of columns and rows:
  lcd.begin(20, 4);
  
  for(i=0; i<DISPLAY_BUFFER_LEN; i++)
    {
      display_buffer[i] = 0x0F;
    }

  //  pinMode(PB0, INPUT);
  pinMode(fxCE, INPUT);
  pinMode(PA8, INPUT);

#if LOGIC_ANALYSER
  // Attach an interrupt to the CE line
  //  attachInterrupt(digitalPinToInterrupt(PB0), ce_isr, FALLING);
  attachInterrupt(fxCE, ce1_isr_2, FALLING);
  attachInterrupt(PA8,  ce3_isr_2, FALLING);
#else
  attachInterrupt(fxCE, ce1_isr, FALLING);
  attachInterrupt(fxCE3,  ce3_isr, FALLING);
#endif
  
  lcd.cursor();
}

volatile int f = 0;
volatile int i;


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Loop that dumps trace
//
// In logic analyser mode the signal traces are processed
// In non LA (and LA)  mode the display buffers are sent to the LCD
//

void loop()
{
  int a, j;
  boolean op, oe, we;
  unsigned char addr;
  unsigned char data;
  unsigned char regs1[17];
  boolean in_command = false;

  // Loop flag, shows we are looping
  f = !f;

  // We just display the buffers continuously
  // Interrupt routine might update mid loop, but next loop should
  // correct any incoherency.
  
  lcd.setCursor(0, 0);
  for(i=0; i< DISPLAY_BUFFER_LEN; i++)
    {
      lcd.setCursor(19-i, 0);
      lcd.print(fx702pschar(display_buffer[i]));
    }

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

  // Dump trace to serial port
  // Process data at the same time

#if LOGIC_ANALYSER  
  // CE1, this has the dot matrix character data
  
  while( isr_trace_1_in != isr_trace_1_out )
    {
#if FLAGS
      op = (isr_trace_ce1[isr_trace_1_out].flags >> BITNUM_OP) & 1;
      oe = (isr_trace_ce1[isr_trace_1_out].flags >> BITNUM_OE) & 1;
      we = (isr_trace_ce1[isr_trace_1_out].flags >> BITNUM_WE) & 1;
#else
      op = isr_trace_ce1[isr_trace_1_out].op;
      oe = isr_trace_ce1[isr_trace_1_out].oe;
      we = isr_trace_ce1[isr_trace_1_out].we;
#endif
      data = isr_trace_ce1[isr_trace_1_out].data;
      addr = isr_trace_ce1[isr_trace_1_out].addr;

#if SERIAL_DUMP_REGS
      for(i=0;i<16;i++)
	{
	  Serial.print(reg[i],HEX);
	  Serial.print(" ");
	}
      Serial.println("");
#endif

      proc_regs(op, oe, we, data, addr, reg);

	  
#if SERIAL_REGDUMP
      Serial.print("Reg ");
      Serial.print(addr, HEX);
      Serial.print(" = ");
      Serial.println(data, HEX);
#endif
      
      if( we == false )
	{
	  proc_seven_seg(reg, addr, data);
	  proc_matrix(reg, addr, data);
	  proc_annunciators(reg, addr, data);
	  proc_cursor(reg, addr, data);
	}
      
#if SERIAL_SIGNALS
      Serial.print("1 ");
      Serial.print(addr, HEX);
      Serial.print(" ");
      Serial.print(data, HEX);
      Serial.print(" ");
      Serial.print(oe);
      Serial.print(" ");
      Serial.print(we);
      Serial.print(" ");
      Serial.print(op);
      Serial.println("");
#endif
      
      isr_trace_1_out = (isr_trace_1_out + 1) % NUM_ISR_CE1_TRACE;
    }

  while( isr_trace_3_in != isr_trace_3_out )
    {
#if FLAGS
      op = (isr_trace_ce3[isr_trace_3_out].flags >> BITNUM_OP) & 1;
      oe = (isr_trace_ce3[isr_trace_3_out].flags >> BITNUM_OE) & 1;
      we = (isr_trace_ce3[isr_trace_3_out].flags >> BITNUM_WE) & 1;
#else
      op = isr_trace_ce3[isr_trace_3_out].op;
      oe = isr_trace_ce3[isr_trace_3_out].oe;
      we = isr_trace_ce3[isr_trace_3_out].we;
#endif
      data = isr_trace_ce3[isr_trace_3_out].data;
      addr = isr_trace_ce3[isr_trace_3_out].addr;

      // CE3 controller seems to only be commands, no writes

      if( op == 0 )
	{
	  switch(addr)
	    {
	    case 2:
	    case 3:
	      ce3_23_reg = true;
	      ce3_ab_reg = false;
	      ce3_45_reg = false;
	      break;

	    case 4:
	    case 5:
	      ce3_23_reg = false;
	      ce3_45_reg = true;
	      ce3_ab_reg = false;
	      
	      break;
	      
	    case 0xA:
	      ab_index = !ab_index;
	      
	    case 0xB:
	      ce3_23_reg = false;
	      ce3_ab_reg = true;
	      ce3_45_reg = false;	      
	      break;
	    }

	  ce3_reg_value[addr] = data;
	}
      else
	{
	  if( ce3_23_reg )
	    {
	      ce3_reg_23_data[addr] = data;
	    }
	  if( ce3_ab_reg )
	    {
	      if( ab_index )
		{
		  ce3_reg_ab2_data[addr] =data;
		}
	      else
		{
		  ce3_reg_ab_data[addr] = data;
		}
	    }
	  if( ce3_45_reg )
	    {
	      ce3_reg_45_data[addr] = data;
	    }

	}
      
#if SERIAL_CE3
      Serial.print("CE3 Reg:");

      for(i=0;i<16;i++)
	{
	  Serial.print(ce3_reg_value[i], HEX);
	}
      Serial.println("");

      Serial.print("CE3 23 Data:");
      for(i=0;i<16;i++)
	{
	  Serial.print(ce3_reg_23_data[i], HEX);
	}
      Serial.println("");
      
      Serial.print("CE3 45 Data:");
      for(i=0;i<16;i++)
	{
	  Serial.print(ce3_reg_45_data[i], HEX);
	}
      Serial.println("");
      
      Serial.print("CE3 AB  Data:");
      for(i=0;i<16;i++)
	{
	  Serial.print(ce3_reg_ab_data[i], HEX);
	}
      Serial.println("");
      
      Serial.print("CE3 AB2 Data:");
      for(i=0;i<16;i++)
	{
	  Serial.print(ce3_reg_ab2_data[i], HEX);
	}
      Serial.println("");

#endif

      // F2 has it's own flag, F1 isn't as clean, so we have to
      // perform some logic
      
      // F1 is on if it's flag is on or HYP or ARC
      // If F2 is on then F1 is off
      lcd.setCursor(5,3);
      if( (((ce3_reg_23_data[3] ==4)&&(ce3_reg_23_data[2] == 7))||((ce3_reg_23_data[3] ==0xA)&&(ce3_reg_23_data[2] == 2)) || (ce3_reg_ab_data[10] & 8) || (ce3_reg_ab_data[9] & 8)) && !(ce3_reg_ab_data[12] & 8) )
	{
	  lcd.print("F1 ");
	}
      else
	{
	  lcd.print("   ");
	}

      if( (ce3_reg_ab_data[12] & 8) )
	{
	  lcd.print("F2 ");
	}
      else
	{
	  lcd.print("   ");
	}

      if( (ce3_reg_ab_data[9] & 8) )
	{
	  lcd.print("HYP ");
	}
      else
	{
	  lcd.print("    ");
	}

      if( (ce3_reg_ab_data[10] & 8) )
	{
	  lcd.print("ARC ");
	}
      else
	{
	  lcd.print("    ");
	}
      
#if SERIAL_SIGNALS
      Serial.print("3 ");
      Serial.print(addr, HEX);
      Serial.print(" ");
      Serial.print(data, HEX);
      Serial.print(" ");
      Serial.print(oe);
      Serial.print(" ");
      Serial.print(we);
      Serial.print(" ");
      Serial.print(op);
      Serial.println("");
#endif
      
      isr_trace_3_out = (isr_trace_3_out + 1) % NUM_ISR_CE3_TRACE;
    }

  if( overflow_1 )
    {
      Serial.println("Overflow 1");
    }

  if( overflow_3 )
    {
      Serial.println("Overflow 3");
    }

#endif
  
  lcd.setCursor(0,3);
  int v = 0;
  if( seven_seg[3] != 0xF )
    {
      seven_seg[3] = seven_seg[3] & 3;
      for(j=3;j>=0;j--)
	{
	  lcd.print(seven_seg[j]);
#if FLAG_7SEG_COLON
	  Serial.print(":");
#endif
	}
    }
  else
    {
      lcd.print("        ");
    }


#if LCD_ANNUNCIATORS
  lcd.setCursor(0, 2);
  
  for(j=1;j<=0xE;j++)
    {
#if SERIAL_ANN
      Serial.print(annunciators[j]);
      Serial.print(" ");
#endif
      if( annunciators[j] )
	{
	  lcd.print(annunciators[j],HEX);
	}
      else
	{
	  lcd.print(" ");  
	}
    }
#if SERIAL_ANN
  Serial.println("");
#endif
#endif
  

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

  lcd.setCursor(16,1);
  if( annunciators[12] )
    {
      lcd.print("STOP");
    }
  else
    {
      lcd.print("    ");
    }

  lcd.setCursor(12, 1);
  switch( annunciators[13] )
    {
    case 0xB:
      lcd.print("RUN");
      break;

    default:
      lcd.print("WRT");
      break;
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

  //  Serial.println((PIN_MAP[fxD0].gpio_device)->regs->IDR);
  delay(1);

#if 0
  for(i=19; i>=0;i--)
    {
      Serial.print(fx702pschar(display_buffer[i]));
    }
  Serial.println("");  

  for(i=19; i>=0;i--)
    {
      Serial.print(display_buffer[i], HEX);
      Serial.print(" ");  
    }
  Serial.println("");

  for(i=0; i<16;i++)
    {
      Serial.print(reg[i]);
      Serial.print(" ");
    }
  Serial.println("");

  for(i=3;i>=0;i--)
    {
      Serial.print(seven_seg[i]);
      Serial.print(":");
    }
  Serial.println("");
    
#endif
}


