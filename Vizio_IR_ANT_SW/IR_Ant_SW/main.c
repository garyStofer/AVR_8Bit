/*
 * GccApplication1.c
 *
 * Created: 7/12/2012 2:42:13 PM
 *  Author: gary
 */ 

#define F_CPU        16000000 
#include <avr/io.h>		// include I/O definitions (port names, pin names, etc)
#include <avr/wdt.h>
#include <avr/power.h>
#include <avr/interrupt.h>	// include interrupt support
#include <stdbool.h>
#include <util/delay.h>

#define BUTTONS_BUTTON1    0x80
#define LEDS_LED1        (1 << 6)
#define SERVO_POS_MIDDLE (0x400-105)
#define SERVO_POS_LEFT  (SERVO_POS_MIDDLE + 35)
#define SERVO_POS_RIGHT (SERVO_POS_MIDDLE - 35)

// VIZIO IR remote control signal:
// No signal/end of sequence  == high on data in pin PC7
// initial start sequence 8ms low, then 13.4ms high
// followed by 32 bits of Pulse width data sequenced by 0.6ms SPACE. Mark for logic_0 = 0.6ms, MARK for logic_1 = 1.68ms

#define VIZIO_GREEN_BTN  0xaa55fb04		
#define VIZIO_RED_BTN 0xad52fb04
 static  void 
LEDs_Init(void)
{
	DDRD  |=  LEDS_LED1;
	PORTD &= ~LEDS_LED1;
}

static  void 
LEDs_TurnOnLEDs(const uint8_t LEDMask)
{
	PORTD |= LEDMask;
}

static  void 
LEDs_TurnOffLEDs(const uint8_t LEDMask)
{
	PORTD &= ~LEDMask;
}

static  void 
Buttons_Init(void)
{
	DDRD  &= ~BUTTONS_BUTTON1;
//	PORTD |=  BUTTONS_BUTTON1;
}

static bool 
IsButtonPressed( unsigned char bt )
{
	return ((PIND & bt) == 0);
}

volatile uint8_t Signal_captured = 0;

/* for debugging of IR code sequence
#define N_CAPTURES 60
volatile uint8_t capture_lenght = 0;
volatile uint8_t capture_data[N_CAPTURES] = {0};
volatile uint8_t capture_index = 0;
*/
volatile long unsigned IR_code;
volatile unsigned char IR_code_ndx; 

// Todo: should run timer 0 continuously and use it to generate a timeout of about 1 second after a signal 
// is captured and the servo was commanded. This timeout is then used to turn the servo pulse off in case the 
// servo is loaded by the switch and prevents it from jittering.   


int main(void)
{
	unsigned char tmp;
	
	wdt_disable();		/* Disable watchdog if enabled by bootloader/fuses */
	 
	Buttons_Init();
	LEDs_Init();
	clock_prescale_set(clock_div_1);

	// Timer 0 setup for simple count mode, used as timebase for IR-Signal capture
	TCCR0A = 0;		// simple count more 		
	TCCR0B = 5;		// system Clock 16Mhz/1024 = 64us per counter tick 
	TIMSK0 = 0x1;	// Enable overflow interrupt enable 
	TIFR0 = 0;		// clear Overflow interrupt flag before enabling interrupts
	EIFR =0 ;
	DDRC = 0;		// all inputs
	PORTC = 0x80;	// pull-up on PC7
	MCUSR =0;
	
	// Timer 1 setup for  fast PWM mode for servo pulse generation 1 to 2ms 
	DDRB = 0x80;	// PB7 = output OC1_C
	TIMSK1 = 0;		// timer 1 running with no interrupts
	TCCR1A = 0x0F;	// use OC1_C pin for servo pulse
	TCCR1B = 0x0C;	// Fast PWM 10bit, clock/256
	OCR1C = SERVO_POS_RIGHT;
	

	sei();
	
	// set initial state 
	Signal_captured = 1;
	IR_code = VIZIO_GREEN_BTN;
	
    while(1)
    {
		// for manual toggling via HWB button

































































		if (IsButtonPressed(BUTTONS_BUTTON1) )
		{

			// de-bounce -- 5 consecutive reads of switch open 
			for (tmp =0; tmp<=5; tmp++)
			{
				_delay_ms(1);
				if (IsButtonPressed(BUTTONS_BUTTON1))
					tmp = 0;
			}
			
			if (IR_code == VIZIO_GREEN_BTN )
				IR_code = VIZIO_RED_BTN;
			else
				IR_code = VIZIO_GREEN_BTN;
			Signal_captured = 1;	
				
		}			
				
		if ( Signal_captured )
		{
			 switch (IR_code)
			 {
				 case VIZIO_GREEN_BTN:		// Vizio Code Green button
					 LEDs_TurnOnLEDs(LEDS_LED1);
					 OCR1C = SERVO_POS_LEFT;
					 break;
					 
				 case VIZIO_RED_BTN:		// Vizio Code Red button
					 LEDs_TurnOffLEDs(LEDS_LED1);
					 OCR1C = SERVO_POS_RIGHT;
					 break;
			 }
			 Signal_captured = 0;
		}				
    }
}

ISR( INT4_vect,ISR_BLOCK )
{	
	uint8_t cnt;
	
	if ( PINC & (1<<PINC7) )
	{
		Signal_captured = 0;		
		// trigged  by raising edge of signal
		TCNT0 = 0;
		TCCR0B = 5;  // system Clock 16Mhz/1024 = 64us per counter tick 
		// change interrupt edge to falling
		EICRB = 0x2;
	}
	else //	read_counter0()
	{		// trigged  by falling edge of signal
		
		if ((cnt = TCNT0) > 40 )	 // the initial start pulse
		{
			IR_code = 0;
			IR_code_ndx = 0;
		}
		else
		{
			if (cnt > 16)
				IR_code += 1L<<IR_code_ndx;  
			IR_code_ndx++;
		}
		/* for signal analysis only
		if (capture_index < N_CAPTURES-1)
		{
			capture_data[capture_index++]=cnt;
		} */			
		TCCR0B = 0; // stop clocking
		// set interrupt to raising edge trigger
		EICRB = 0x3;
	}
		
}




ISR(TIMER0_OVF_vect, ISR_BLOCK)
{
	Signal_captured = 1; 
	/*
	capture_lenght = capture_index; 
	capture_index = 0;
	*/
	TCCR0B = 0;		// stop counter from  clocking
	EIFR = 0 ;		// clear external interrupt flag 
	EICRB = 0x3;	// set interrupt to raising edge trigger
	EIMSK = 0x10;	// enable INT4 (pin PC7 alternate function)
}	