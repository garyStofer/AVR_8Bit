/*
 * Program for Binary Clock using atmega32U2 running on 16Mhz xtal.
 * 
 Provides a simple clock that reads out the current time via a series of 12 LEDs indicating in binary notation 
 Hours, 10s of minutes and 1s of minutes. 
 
 LEDs are pulse with modulated at a ~16ms interval to either be dimly lit to indicate the "off" state, or brightly lit
 to indicate the "on" state. Using 8 bit Timer1 fed by a 64us clock and running in "normal mode" the timer overflow interrupt 
 produces a ~16ms PWM interval with 256 64us steps. Output comapre A and B are used to time the dim and bright LEDs "ON" times.   
 
 Hardware timer1 is used to generate a 1 HZ interrupt source which then gets used to count up the minutes and hours for
 a 12hour AM/PM display.
 
 *
 *
 * Created: 4/12/2013 2:42:13 PM
 * Author: Gary Stofer
 */ 

#define F_CPU        16000000 
#include <avr/io.h>		// include I/O definitions (port names, pin names, etc)
#include <avr/wdt.h>
#include <avr/power.h>
#include <avr/interrupt.h>	// include interrupt support
#include <avr/eeprom.h>
#include <stdbool.h>
#include <util/delay.h>

#define BUTTON1 (1<<7)			// On Port D
#define BUTTON2 (1<<4)			// on Port D
#define BUTTON3 (1<<5)			// on Port D

#define LEDS_LED1        (1 << 6) // on portD
#define LEDS_HOURS		0x0f	// on PORT D
#define LEDS_1MINS		0xf0	// on Port B
#define LEDS_10MINS		0xe0	// on Port C
#define LEDS_AM_PM		0x10	// on Port C
#define LEDS_Second		0x04	// on Port C


#define LED_BRIGHT  100	// 0 == max brightness 
#define LED_DIMM	250 // 255 == min brightness, i.e OFF

// global vars
unsigned char Seconds = 0;
unsigned char Minutes = 0;
unsigned char Hours = 0;
unsigned char am_PM	 =0;

struct ee_data 
{
	unsigned char dim_level;
	unsigned char bright_level;

} EE_data;


static  void 
LEDs_Init(void)
{
	DDRD |= LEDS_LED1;	// LED on pcb
	DDRB |= LEDS_1MINS;		// PB4..7
	DDRD |= LEDS_HOURS;		// PD0..3
	DDRC |= LEDS_10MINS;	// PC5..7
	DDRC |= LEDS_AM_PM;		// PC2
	DDRC |= LEDS_Second;	// PC4
}


static  void 
TurnOffAllLEDs()
{
	PORTD = (BUTTON1 | BUTTON2 | BUTTON3); // maintain the pull-ups for switches
	PORTB =0;
	PORTC =0;
	
}

static  void 
Buttons_Init(void)
{
	DDRD  &= ~ (BUTTON1 | BUTTON2 | BUTTON3);
	PORTD = (BUTTON1 | BUTTON2 | BUTTON3); // for pull ups
}

static bool 
IsButtonPressed( unsigned char bt )
{
	return ((PIND & bt) == 0);
}

//Function to ripple the change in seconds through minutes and hours
static void
ripple( void)
{
	if( Seconds >= 60)
	{
		Seconds = 0;
		Minutes++;
	}	  
	
	if (Minutes >=60)
	{
		Minutes = 0;
		Hours++;
	}
	
	if (Hours >=12)
	{
		Hours = 0;
		am_PM++;
	}
	
	if (am_PM >1)
	{
		am_PM = 0;
	}		
	
}
static unsigned mode = 1;		// the operational mode of the clock , 0=running,1=clock-setting,2=dim-setting, 3=bright setting
  
unsigned volatile char tmp;
int main(void)
{

	unsigned char tmp;
	
	wdt_disable();		/* Disable watchdog if enabled by bootloader/fuses */
	power_usb_disable() ;
	power_usart1_disable();
	power_spi_disable();
	
	Buttons_Init();
	LEDs_Init();
	clock_prescale_set(clock_div_1);	// run at x-tal frequency 16Mhz
	
	
	eeprom_read_block( &EE_data,0,sizeof(EE_data));
		
	if (EE_data.bright_level == 0xff || EE_data.dim_level == 0xff)
	{
		EE_data.bright_level = LED_BRIGHT;
		EE_data.dim_level = LED_DIMM;
		eeprom_write_block(&EE_data,0,sizeof(EE_data));
		eeprom_busy_wait();
	}
	
	OCR0A = EE_data.bright_level;
	OCR0B = EE_data.dim_level;

	// Timer 0 setup for simple count mode, used as timebase LED PWM
	TCCR0A = 0;		// simple count more 		
	TCCR0B = 4;		// system Clock 16Mhz/256 = 16us per counter tick 
	//TCCR0B = 3;		// system Clock 16Mhz/64 = 4us per counter tick 
	//TCCR0B = 5;		// system Clock 16Mhz/1024 = 64us per counter tick 
	//TCCR0B = 2;		// system Clock 1Mhz/8 = 8us per counter tick 
	TIMSK0 = 0x7;	// Enable OverFLow , OCR0A and OCR0B interrupt enables
	
	MCUSR =0; //MCU status register clear 
	
	// Timer 1 setup for  fast PWM mode for servo pulse generation 1 to 2ms 

	TIMSK1 = 0x2;		// Enable OCR1A interrupt 
	TCCR1A = 0x00;		// No pin toggles on compare -- CTC (normal) mode
	TCCR1B = 0x0D;		// CTC mode, F_CPU 16mhz/1024 clock ( 64us)
	//TCCR1B = 0x0B;		// CTC mode, F_CPU 1Mhz/64 clock ( 64us)
	OCR1A = 15625-1;	// counting 15625 counts of 64 us == 1 second

	sei();
		
    while(1)	// for ever
    {
		
		if (IsButtonPressed(BUTTON1) )
		{

			// de-bounce -- 5 consecutive reads of switch open 
			for (tmp =0; tmp<=5; tmp++)
			{
				_delay_ms(1);
				if (IsButtonPressed(BUTTON1))
					tmp = 0;
			}
			
			if ( mode < 3)
				mode++;
			else
			{
				eeprom_write_block(&EE_data,0,sizeof(EE_data));
				eeprom_busy_wait();
				mode = 0; // enter running mode
			}				
		
		}			
				
		if (IsButtonPressed(BUTTON2) )
		{

			// de-bounce -- 5 consecutive reads of switch open
			for (tmp =0; tmp<=5; tmp++)
			{
				_delay_ms(1);
				if (IsButtonPressed(BUTTON2))
				tmp = 0;
			}
			
			switch (mode)
			{
				case 1:
					Minutes++;
					ripple();
					break;
					
				case 2:
					if (OCR0B < 0xff )		// dim level
					{
						OCR0B++;
						EE_data.dim_level = OCR0B;
					}						
					break;
					
				case 3:						// bright level
					if (OCR0A < (OCR0B +10) )
					{
						OCR0A +=10;
						EE_data.bright_level =OCR0A;
					}						
					break;
			}			
			
		}	
		
		if (IsButtonPressed(BUTTON3) )
		{

			// de-bounce -- 5 consecutive reads of switch open
			for (tmp =0; tmp<=5; tmp++)
			{
				_delay_ms(1);
				if (IsButtonPressed(BUTTON3))
				tmp = 0;
			}
						
			switch (mode)
			{
				case 1:
					Hours++;
					ripple();
					break;
					
				case 2:
					if (OCR0B > OCR0A)		// dim level
					{	

						OCR0B--;
						EE_data.dim_level = OCR0B;
					}						
					break;
					
				case 3:						// bright level
					if (OCR0A >50)	
					{
						OCR0A -=10;
						EE_data.bright_level =OCR0A;
					}						
					break;
			}
		}
				
    } // end of for-ever
}


ISR(TIMER0_COMPA_vect,ISR_BLOCK)	// Turn on LEDS that should be bright
{
	PORTD |= Hours & LEDS_HOURS;
	PORTB |= ((Minutes %10)<<4) & LEDS_1MINS;	// 1'S OF MINUTES
	PORTC |= ((Minutes/10)<<5) & LEDS_10MINS ;	// 10's of minutes
	PORTC |= (am_PM << 4) & LEDS_AM_PM ;	//AM-PM indication
	PORTC |= ((Seconds%2) << 2) & LEDS_Second;
	PORTD |= ((Seconds%2) << 6) & LEDS_LED1;	// Green led on PCB
}


ISR(TIMER0_COMPB_vect,ISR_BLOCK)	// Turn on LEDS that should be dim, thats all LEDS 
{
	PORTD |= LEDS_HOURS;
	PORTB |= LEDS_1MINS;	
	PORTC |= LEDS_10MINS;	
	PORTC |= LEDS_AM_PM;	
	PORTC |= LEDS_Second;
}


ISR(TIMER0_OVF_vect, ISR_BLOCK)
{
	TurnOffAllLEDs();
}	



ISR(TIMER1_COMPA_vect, ISR_BLOCK)
{
	if (mode)		// clock is in settings mode, don't increment time
	{
		Seconds = 0;
		return;
	}	
	Seconds++;
	ripple();
}