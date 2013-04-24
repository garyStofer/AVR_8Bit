/*
 * SPI_xfer.c
 *
 * Created: 6/7/2012 10:33:51 AM
 *  Author: gary
 *   
 * Function to shift out 8 bits of data on the USI DO port, MSB first, clocked with positive edge on SCKL 
 * This function is in it's own file so that Optimization can be controlled specifically for this function 
 */ 
#include <avr/io.h>

// #pragma GCC optimize "-Ofast" 
#pragma GCC optimize "-Ofast" 

unsigned char
SPI_Xfer(unsigned char data)
{
	
	USIDR = data;
	USISR = _BV(USIOIF); // clear flag and bit counter
	
	while ( (USISR & _BV(USIOIF)) == 0 )
	{
		// 3wire mode,No interrupts,externalClock positive edge via USITC toggle
		USICR = (1<<USIWM0)|(1<<USICS1)|(1<<USICLK)|(1<<USITC);
	}
	return USIDR;
}

/*
// This could be coded straight down to improve the speed
SPI_xfer_Fast:
out USIDR,r16
ldi r16,(1<<USIWM0)|(1<<USICS1)|(1<<USICLK)|(1<<USITC)
out USICR,r16 ; MSB
out USICR,r16
out USICR,r16
out USICR,r16
out USICR,r16
out USICR,r16
out USICR,r16
out USICR,r16
out USICR,r16
out USICR,r16
out USICR,r16
out USICR,r16
out USICR,r16
out USICR,r17
out USICR,r16 ; LSB
out USICR,r16
in r16,USIDR
ret
*/