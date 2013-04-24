/*
 *ATtiny / TLC5947 LEDs
 *
 * Created: 6/4/2012 9:40:15 PM
 *  Author: gary
 */ 

/*
ATtiny 4313 Fuse settings: 
~-~-~-~-~-~-~-~-~-~-~-~-~-~-
EXTENDED = 0xFF 
HIGH = 0xDF, or 0x5F for DebugWire enable
LOW = 0xE4 
*/

#define F_CPU 8000000UL
#include <avr/io.h>
#include <inttypes.h>
#include <string.h>
#include <stdbool.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>



#define XLAT_LOW()	PORTD &= ~_BV(PORTD0)
#define XLAT_HIGH()	PORTD |=  _BV(PORTD0)
#define BLANK_LOW()	PORTB &= ~_BV(PORTB4)	
#define BLANK_HIGH()PORTB |=  _BV(PORTB4)


#define N_LEDS	8
#define N_GRAYSCALE 12		// NOTE: These are only the Gray scale steps 1based , additionally there is 0 for all off and 13 for all on (MAX brightness)
#define LEDS_OFF 0
#define LEDS_MAX N_GRAYSCALE+1

#define FRAMES_P_SECOND	48	// evenly divisible by 8 and 24 
#define FRAME_EXPOSURE_MS	20	// about 20 ms exposure per frame

typedef enum Colors {BLU=0,RED,GRN,N_COLOR} t_Color;
typedef enum Direction {CCW,CW,ALTERNATE} t_dir;
typedef enum Up_Down { TRIANGLE=0,UP,DOWN=-1} t_up_down;
uint8_t LedArray[N_LEDS][N_COLOR];	
uint8_t EEMEM EE_flashSequence_index;	 
	
extern unsigned char SPI_Xfer( unsigned char );


void 
HW_init(void)
{
	int i;

	cli();
	// set CPU clock divider to 1
	// CLOCK source is selected by fuses and can not be  changed in the program
	// Set fuses to 8Mhz and NO CLKDIV

	// Watchdog 
	// Watchdog is controlled by a fuse setting and can not be disabled by a program when the fuse is set
	// Make sure the Watchdog fuse is not set 

	// Port directions
	DDRD |= _BV(PORTD0); //PD0 as output for TLC5947 XLAT signal  
	DDRB |= _BV(PORTB4); //PB4 as output for TLC5947 Blank signal
	DDRB |= _BV(PORTB6); //PB6 as output for USI Pin DO, connected to TLC5947 SIN
	DDRB |= _BV(PORTB7); //PB7 as output for USI pin USCK, connected to TLC5947 SCLK
	
	PORTB = 0;			// Make sure DO and USCK are low to begin with 

	BLANK_HIGH();	// Turn All The LEDs off -- nor really needed 
	
	// set all GrayScale values to 0 in the TLC
	for (i = 0; i < 36; i++)
	{
		SPI_Xfer(0);
	}
	
	XLAT_HIGH();	// Latch it to the outputs
	XLAT_LOW();		// 
	
	BLANK_LOW();	// Turn all The LEDs active 

	// Timer1 setup in delay function 

	// USI config for 3wire SPI master mode in spiXfer()
}


// Simple blocking delay() using Timer/Counter1 (16bit) counts with clock pre-scaler set to divide by 256 for a clock rate of 32us at ~8Mhz system clock 
// Maxiumm delay is 0xffff counst = 0xffff * 32uS ==  2 seconds 
void
delay( int n_ms)
{
	if (n_ms>2000)		// limit to max duration the timer can do 
		n_ms= 2000;
		
	long t = (n_ms*1000L)/32;	// calculate timer counts in 128us steps
	
	TCCR1A = 0; // Normal mode operation
	TCCR1B = 0; // clock stopped
	TCNT1=0;	// Clear counter
	OCR1A=t;	// set count target value
	TIFR |=_BV(OCF1A);		// This clears the compare register flag
	TCCR1B = 0x04; // Enable count clock to be SysClk/256, 1 tick = 32us
	while (! (TIFR & _BV(OCF1A)))
	{
		//block
	}
	
}


void
set_TLC5947_Grayscale(void)
{
	unsigned char *cp;
	signed char lum;
	unsigned short led_a, led_b;
 
	// loop to fill the TLC5947 shift register with 12bit x 24Led data -- Led nomenclature as per TLC5947 pin-out
	cp = &LedArray[N_LEDS-1][N_COLOR-1]; // start at end of array and work backwards-- Must send most sig bit of LED 23 first

	for (cp = &LedArray[N_LEDS-1][N_COLOR-1]; cp >= &LedArray[0][0] ; ) // Process two LEDs per iteration so we end up with 24 bits, i.e. 3 bytes to transfer each
	{
		lum = (*cp--);	
		if (lum == 0)
			led_a = 0;
		else if (lum > N_GRAYSCALE)	
			led_a = 0xfff;
		else	
			led_a = 1 << (lum-1);	// LED23  gray scale lookup, ... LED21, ... .... LED 1
		
		lum = (*cp--);
		if (lum == 0)
			led_b = 0;
		else if( lum > N_GRAYSCALE)
			led_b = 0xfff;
		else 	
			led_b = 1 << (lum-1);	// LED22  gray scale lookup, ... LED20. ... .... LED 0
		
		SPI_Xfer(( led_a & 0x0ff0) >> 4 );	// store 8 most significant bits of 1st LED value into transmit register
		SPI_Xfer(((led_a & 0x00f) << 4 ) | ( (led_b & 0xf00) >>8 ) );  // Store 4 least sig. bits of led a and 4 most sig. bits of led b
		SPI_Xfer(  led_b & 0xff);
	}
	XLAT_HIGH();	// Latch it to the outputs
	XLAT_LOW();		//
}


void
rotate_one_led( t_dir dir)	// Rotate the entire LED matrix by one tri-color LED position (8 tri-color leds) 
{
	// Rotate the pattern CCW
		short n;
		unsigned char r,b,g;
						
		if (dir == CCW)
		{
			b = LedArray[0][BLU];
			r = LedArray[0][RED];
			g = LedArray[0][GRN];
			for(n =0; n < N_LEDS-1; n++)
			{
				LedArray[n][BLU] = LedArray[n+1][BLU];
				LedArray[n][RED] = LedArray[n+1][RED];
				LedArray[n][GRN] = LedArray[n+1][GRN];
				
			}
			LedArray[n][BLU] = b;
			LedArray[n][RED] = r;
			LedArray[n][GRN] = g;
		}
		else
		{
			b = LedArray[N_LEDS-1][BLU];
			r = LedArray[N_LEDS-1][RED];
			g = LedArray[N_LEDS-1][GRN];
			for(n=N_LEDS-2; n >=0; n--)
			{
				LedArray[n+1][BLU] = LedArray[n][BLU];
				LedArray[n+1][RED] = LedArray[n][RED];
				LedArray[n+1][GRN] = LedArray[n][GRN];
				
			}
			LedArray[0][BLU] = b;
			LedArray[0][RED] = r;
			LedArray[0][GRN] = g;	
		}				
}


void
rotate_led_color(void)
{
	// Rotate the LED matrix by one color . i.e Green becomes Red & Red => Blue & Blue => Green -- only one diection
	unsigned char n;
	unsigned char t;
	for(n =0; n < N_LEDS; n++)
	{
		t =LedArray[n][0];
		LedArray[n][0] = LedArray[n][1];
		LedArray[n][1] = LedArray[n][2];
		LedArray[n][2] = t;
	}
	
}
/* Function to run a light patter in a circular motion, changing direction of rotation and colors along the way.
 *	
 
   Arguments: 
		direction: The sense of rotation ClockWise, Counter Clock Wise or Alternating
		speed:     The speed of the chasing pattern, low numbers == faster speed. E.g. settng this to Frames_per_Second
		           and the pattern will advance one position per second. Using 0 the pattern will not appear to move
		interval:  time until the color and the direction changes (if set to Alternate). 
				   Note: the colors are swapped in one direction only. If color mixing is present i.e. more than
				         one color turned on the array, rotating through the colors will yield 3 distinct colors.
		duration:  overall duration of execution of the pattern. If set to 0 the pattern continues indefinitely  				   

 */

void
RoundAbout(t_dir direction, uint8_t speed , uint8_t interval_S, uint16_t duration)
{
	short i;
	t_dir dir; 
	
	if (direction == ALTERNATE ) 
		dir = CW;
	else 
		dir = direction;

	duration *= FRAMES_P_SECOND;		// Duration of the run in frame counts
	for(;;) 
	{
			 
		for (i = 0; i < FRAMES_P_SECOND * interval_S; i++)	// run for n seconds
		{
			if (duration)			// if duration is not zero decrement and return when elapsed
			{
				if (--duration == 0)
					return;
			}

			// Advance the pattern by one LED position
			if ( i % speed == 0)
			{
				rotate_one_led(dir);
				set_TLC5947_Grayscale();	// Send LED pattern out to the LED driver chip
			}
		
			delay( FRAME_EXPOSURE_MS);  // about 20 ms exposure per frame
		}
		rotate_led_color();
		if (direction == ALTERNATE ) 
		{
			dir++;
			if (dir == ALTERNATE)
				dir =0;
		}

	
	}
}


/* Function to run flash all LEDs in a ascending or decaying mode.
 *	
   Arguments: 
		BR:		The Maximum brightness of the LEDs. 
		R,G,B	boolean flags indicating the use of the individual colors, 6 colors plus white possible
		delay:  In ms, the rate of pulsating. The pattern pulsates at (N_LEDS+2) * delay rate. 
		up_down: -1 == initially bright then decaying, +1 == initially off then ascending, 0 == ascend then decay.
				 There is an extra delay when at 0 intensity.
		
*/	
	
void
Flash(uint8_t BR, _Bool R, _Bool G, _Bool B, uint8_t delay_ms, t_up_down up_down )
{
	int8_t br; 
	int8_t u_d;

	if (up_down == TRIANGLE)	// up, then down, then up
		u_d= 1;
	else
		u_d = up_down;
		
	memset(LedArray,0,sizeof(LedArray)); 
	
	for (;;)//ever
	{
		if (up_down)
		{
			if ( br < LEDS_OFF)
			{
				br = BR;
			}			
			else if (br > BR)
			{
				br = LEDS_OFF;
			}			
		}
		else
		{
			if(br <= LEDS_OFF+1)
			{
				u_d *= -1;
			}
			else if ( br > BR)
			{
				u_d *= -1;
			}
		}
		
		for( int i =0 ; i<N_LEDS;i++)
		{
			LedArray[i][RED] = br*R;
			LedArray[i][GRN] = br*G;
			LedArray[i][BLU] = br*B;
		}
		set_TLC5947_Grayscale();
		delay(delay_ms);
		
		if (br == 0)			// Give extra delay when lights are out
		{
			delay(delay_ms);	
		}
		br += u_d;
	}	
	
}

// Heartbeat 
void
ThumpThump( uint8_t delay_ms, t_Color col1, t_Color col2, t_Color col3 )
{
	int8_t n;
	
	
	for(;;)
	{
		memset(LedArray,0,sizeof(LedArray)); 
		for ( n=13; n>7; n--)
		{
			for( int i =0 ; i<N_LEDS;i++)
			{
				LedArray[i][col1] = n;
			}
			set_TLC5947_Grayscale();
			delay(delay_ms/2);
		}	
		memset(LedArray,0,sizeof(LedArray)); 
		for (; n<10; n++)
		{
			for( int i =0 ; i<N_LEDS;i++)
			{
				//LedArray[i][col1] = n;
				LedArray[i][col2] = n;
			}
			set_TLC5947_Grayscale();
			delay(delay_ms/2);
		}
		memset(LedArray,0,sizeof(LedArray)); 
		for (; n>=LEDS_OFF; n--)
		{
			for( int i =0 ; i<N_LEDS;i++)
			{
				//LedArray[i][col1] = n;
				//LedArray[i][col2] = n;
				LedArray[i][col3] = n;
			}
			set_TLC5947_Grayscale();
			delay(delay_ms);
		}
		delay(delay_ms*2);

	}
	
}

int 
main(void) 
{
	unsigned int i;
	uint8_t fl_seq;

	HW_init();

	delay(1000);	 // Power on good and solid until ee program
	fl_seq = eeprom_read_byte(&EE_flashSequence_index);		// get the last sequence number from the EEPROM and advance to the next 
	eeprom_write_byte(&EE_flashSequence_index,++fl_seq);	// store the next seq number in EEPROM 
	
	
	switch (fl_seq)
	{
		case 1:
			//ThumpThump( uint8_t delay_ms, t_Color col1, t_Color col2, t_Color col3 ) 
			ThumpThump(80,RED, RED,RED);
			break;
		
		case 2:
			//ThumpThump( uint8_t delay_ms, t_Color col1, t_Color col2, t_Color col3 )
			ThumpThump(80,GRN, GRN,GRN);
			break;
		
		case 3:
			//ThumpThump( uint8_t delay_ms, t_Color col1, t_Color col2, t_Color col3 )
			ThumpThump(80,RED, GRN,BLU);
			break;
			
		case 4:
			//Flash(uint8_t BRirightness, _Bool R, _Bool G, _Bool B, uint8_t delay_ms, t_up_down up_down )
			Flash( 12,0,0,1,70,UP);
			break;
		
		case 5:
			//Flash(uint8_t BRirightness, _Bool R, _Bool G, _Bool B, uint8_t delay_ms, t_up_down up_down )
			Flash( 12,0,1,0,70,DOWN);
			break;
			
		case 6:
			//Flash(uint8_t BRirightness, _Bool R, _Bool G, _Bool B, uint8_t delay_ms, t_up_down up_down )
			Flash( 12,1,0,0,50,TRIANGLE);
			break;
			
		case 7:
			memset(LedArray,0,sizeof(LedArray)); 
			LedArray[0][BLU] = 3; // long trail	
			LedArray[1][BLU] = 4; 
			LedArray[2][BLU] = 5;	
			LedArray[3][BLU] = 6;	
			LedArray[4][BLU] = 7; 
			LedArray[5][GRN] = 10;	//White head
			LedArray[5][RED] = 10;
			LedArray[5][BLU] = 10;
			//RoundAbout(t_dir direction, uint8_t speed , uint8_t interval_S, uint16_t duration)
			RoundAbout(CW,4,5,0);
			break;
			
		case 8:
			memset(LedArray,0,sizeof(LedArray));
			LedArray[5][BLU] = 3; // long trail
			LedArray[4][BLU] = 4;
			LedArray[3][BLU] = 5;
			LedArray[2][BLU] = 6;
			LedArray[1][BLU] = 7;
			LedArray[0][GRN] = 10;	//White head
			LedArray[0][RED] = 10;
			LedArray[0][BLU] = 10;
			RoundAbout(CCW,4,5,0);
			break;
			
		case 9:
			memset(LedArray,0,sizeof(LedArray)); 
			LedArray[0][GRN] = 4;	
			LedArray[1][GRN] = 10; // trail
			LedArray[2][GRN] = 4;	
			//RoundAbout( direction, speed,interval_S,  duration)
			RoundAbout(ALTERNATE,1,3,0);
			break;
			
		case 10:
			memset(LedArray,0,sizeof(LedArray));
			LedArray[0][GRN] = 3;
			LedArray[1][GRN] = 11; // dot
			LedArray[2][GRN] = 3;
			LedArray[0][RED] = 4;
			LedArray[1][RED] = 11; // dot
			LedArray[2][RED] = 4;
			LedArray[0][BLU] = 6;
			LedArray[1][BLU] = 11; // dot
			LedArray[2][BLU] = 6;
			RoundAbout(CCW,3,4,0);
			break;
	
		case 11:
			memset(LedArray,0,sizeof(LedArray));
			LedArray[0][RED] = 4;
			LedArray[1][RED] = 10; // trail
			LedArray[2][RED] = 4;
			LedArray[0][BLU] = 4;
			LedArray[1][BLU] = 10; // trail
			LedArray[2][BLU] = 4;
			RoundAbout(ALTERNATE,3,4,0);
			break;
			
		default :	// default case -- end of possible choices, indicate by static pattern and reset fl_seq in eeprom to start from the beginning again
			fl_seq = 0;	
			eeprom_write_byte(&EE_flashSequence_index,fl_seq);
			LedArray[0][RED] = 6; 
			LedArray[1][GRN] = 6;
			LedArray[2][BLU] = 6;
			LedArray[3][RED] = 6;
			LedArray[4][GRN] = 6;
			LedArray[5][BLU] = 6;	
			LedArray[6][RED] = 6;
			LedArray[7][GRN] = 6;
			LedArray[7][RED] = 6;
			set_TLC5947_Grayscale();
			break;

	}	
	
	// we should never get to this point, except for the default clause above.	
	for(;;);	//never exit 
  
}



