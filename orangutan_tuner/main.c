/* orangutan_tuner - an application for the Pololu Orangutan SVP
 *
 * This application uses the Pololu AVR C/C++ Library.  For help, see:
 * -User's guide: http://www.pololu.com/docs/0J20
 * -Command reference: http://www.pololu.com/docs/0J18
 *
 * Created: 4/15/2015 8:27:14 PM
 *  Author: Seth
 */

#include <pololu/orangutan.h>
#include <avr/pgmspace.h>
#include <avr/io.h>
#include <stdbool.h>
#include <ffft.h>

const int		NOISE = 30;

bool volatile	g_update_lcd = false;
bool volatile	g_sample_rdy = false;

int volatile	g_adc_output = 0;
int volatile	counter = 0;

int16_t			sample[FFT_N];
short volatile	samplePostion = 0;
complex_t		complexBuffer[FFT_N];
uint16_t		fftOutput[FFT_N/2];

uint16_t		largest_bin_val;
int				predom_freq_bin;

unsigned long volatile	g_prev_time;
unsigned long volatile	g_num_ticks;

int g_max = 0;
int g_min = 1023;

int main()
{
	//play_from_program_space(PSTR(">g32>>c32"));  // Play welcoming notes.

	/* Start Timer 3 to handle LCD update timing */
	TCCR3A = (1 << COM3A1); //Clear compare match A
	TCCR3B = (1 << WGM32) | (1 << CS31) | (1 << CS30); //CTC mode, with prescaler 64
	OCR3A = 31249; //Interrupt at 10Hz
	TIMSK3 = (1 << OCIE3A); //Enable interrupts from Timer 3
	
	/* Start Timer 0 in CTC mode to fire at 15,923.6 Hz to govern ADC */
	TCCR0A = (1 << COM0A1) | (1 << COM0A0) | (1 << WGM01);
	TCCR0B = (1 << CS01); //Prescaler 8
	OCR0A  =157;

	/* Enable the ADC and start the conversion with auto-triggering on Timer 0 Comp A 	
*/
	ADMUX = (1 << REFS0);
	ADCSRA = (1 << ADEN) | /*(1 << ADSC)|*/ (1 << ADATE) | (1 << ADIE) | (1 << ADPS0) | (1 << ADPS1) | (1 << ADPS2);
	ADCSRB = (1 << ADTS1) | (1 << ADTS0);
	DIDR0 = (1 << ADC0D);

	sei(); 

    clear();
	while(1)
	{
		if(g_sample_rdy)
		{
			fft_input(sample, complexBuffer);
			
			//Re-enable the interrupt, get back to sampling
			g_sample_rdy = false;
			samplePostion = 0;
			ADCSRA |= (1 << ADIE);
			
			fft_execute(complexBuffer);
			fft_output(complexBuffer, fftOutput);
			
			largest_bin_val = 0;
			predom_freq_bin = 0;
			
			//Traverse FFT output, find dominant bin
			for (int i=0; i < FFT_N/2; i++)
			{
				int temp = fftOutput[i];
				
				if (temp > largest_bin_val)
				{
					largest_bin_val = temp;
					predom_freq_bin = i;
				}
			}
			
		}
		
		if (g_update_lcd)
		{
			clear();
			
			//lcd_goto_xy(0,0);
			//print_long(predom_freq_bin);
			//lcd_goto_xy(0,1);
			//print_long(largest_bin_val);
			
			/* Used for getting timing of the ADC */
			lcd_goto_xy(0,0);
			print_long(g_num_ticks);
			lcd_goto_xy(0,1);
			print_long(counter);
			
			/* Used for testing microphone gain */
			//lcd_goto_xy(0,0);
			//print_long(g_max);
			//lcd_goto_xy(0,1);
			//print_long(g_min);
			
			g_update_lcd = false;
		}
	}
}

ISR(TIMER3_COMPA_vect)
{
	g_update_lcd = true;
}

ISR(ADC_vect)
{
	/* Find the number of ticks between interrupts */
	long curr_time = get_ticks();
	
	g_num_ticks = curr_time - g_prev_time;
	g_prev_time = curr_time;
	/* It's 208 when run wide open*/
	
	g_adc_output = ADC;
	counter++;
	/* Code used for testing microphone gain */
	//if(g_adc_output > g_max)
		//g_max = g_adc_output;
		//
	//if(g_adc_output < g_min && g_adc_output != 0)
		//g_min = g_adc_output;
	
	if(g_adc_output >= (512 + NOISE) || g_adc_output <= (512 - NOISE))
		sample[samplePostion] = g_adc_output - 512;
	else
		sample[samplePostion] = 0;
		
	samplePostion++;
	
	//If the buffer is full, turn off the interrupt for now
	if (samplePostion >= FFT_N)
	{
		ADCSRA &= ~(1 << ADIE);
		g_sample_rdy = true;
	}
}
