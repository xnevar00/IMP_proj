//			IMP - VUT FIT project
//				  12/2023
//		Veronika Nevarilova (xnevar00)
//				   main.c

/* Header file with all the essential definitions for a given type of MCU */
#include "MK60DZ10.h"
#include <string.h>
#include <stdbool.h>
#include "main.h"	// array with supported characters


/* Macros for bit-level registers manipulation */
#define GPIO_PIN_MASK	0x1Fu
#define GPIO_PIN(x)		(((1)<<(x & GPIO_PIN_MASK)))


/* Constants specifying delay loop duration */
#define	tdelay1			100
#define tdelay2 		10

enum {MESSAGE1, MESSAGE2, UARTMESSAGE};

int msg_uart_length = 0;
char msg_uart[MAXMESSAGELENGTH];
uint8_t to_display[MAXTODISPLAYLENGTH] = {0};
int to_display_length = 0;
char message[MAXMESSAGELENGTH];
char default_message[] = "Hello world!";
char message2[] = "xnevar00";
int activeMessage = MESSAGE1;
bool change = true;
int window_start_index = -16;

void UARTInit(void)
{
    UART5->C2  &= ~(UART_C2_TE_MASK | UART_C2_RE_MASK);
    UART5->BDH =  0x00;
    UART5->BDL =  0x1A; // Baud rate 115 200
    UART5->C4  =  0x0F;
    UART5->C1  =  0x00;
    UART5->C3  =  0x00;
    UART5->MA1 =  0x00;
    UART5->MA2 =  0x00;
    UART5->S2  |= 0xC0;
    UART5->C2 |= UART_C2_RE_MASK;
    UART5->C2  |= ( UART_C2_TE_MASK | UART_C2_RE_MASK );
}

void MCUInit(void)  {
    MCG->C4 |= ( MCG_C4_DMX32_MASK | MCG_C4_DRST_DRS(0x01) );
    SIM->CLKDIV1 |= SIM_CLKDIV1_OUTDIV1(0x00);
}

/* Configuration of the necessary MCU peripherals */
void SystemConfig() {
	/* Turn on all port clocks */
	SIM->SCGC5 = SIM_SCGC5_PORTA_MASK | SIM_SCGC5_PORTE_MASK;
	SIM->SCGC1 |= SIM_SCGC1_UART5_MASK;


	/* Set corresponding PTA pins (column activators of 74HC154) for GPIO functionality */
	PORTA->PCR[8] = ( 0|PORT_PCR_MUX(0x01) );  // A0
	PORTA->PCR[10] = ( 0|PORT_PCR_MUX(0x01) ); // A1
	PORTA->PCR[6] = ( 0|PORT_PCR_MUX(0x01) );  // A2
	PORTA->PCR[11] = ( 0|PORT_PCR_MUX(0x01) ); // A3

	/* Set corresponding PTA pins (rows selectors of 74HC154) for GPIO functionality */
	PORTA->PCR[26] = ( 0|PORT_PCR_MUX(0x01) );  // R0
	PORTA->PCR[24] = ( 0|PORT_PCR_MUX(0x01) );  // R1
	PORTA->PCR[9] = ( 0|PORT_PCR_MUX(0x01) );   // R2
	PORTA->PCR[25] = ( 0|PORT_PCR_MUX(0x01) );  // R3
	PORTA->PCR[28] = ( 0|PORT_PCR_MUX(0x01) );  // R4
	PORTA->PCR[7] = ( 0|PORT_PCR_MUX(0x01) );   // R5
	PORTA->PCR[27] = ( 0|PORT_PCR_MUX(0x01) );  // R6
	PORTA->PCR[29] = ( 0|PORT_PCR_MUX(0x01) );  // R7

	/* Set corresponding PTE pins (output enable of 74HC154) for GPIO functionality */
	PORTE->PCR[28] = ( 0|PORT_PCR_MUX(0x01) ); // #EN

	/* Change corresponding PTA port pins as outputs */
	PTA->PDDR = GPIO_PDDR_PDD(0x3F000FC0);

	/* Change corresponding PTE port pins as outputs */
	PTE->PDDR = GPIO_PDDR_PDD( GPIO_PIN(28));


	// UART5 Rx and Tx pins
	PORTE->PCR[8] = (0 | PORT_PCR_MUX(0x03)); // TX
	PORTE->PCR[9] = (0 | PORT_PCR_MUX(0x03)); 	// RX

	PORTE->PCR[26] = 0x010A0103;		// SW5 for changing displayed message
	PORTE->PCR[27] = 0x010A0103;		// SW4 for changing displayed message

	NVIC_ClearPendingIRQ(PORTE_IRQn);

	UARTInit();

	NVIC_EnableIRQ(PORTE_IRQn);			// enable interrupts on port E

	// timer
	SIM->SCGC6 |= SIM_SCGC6_PIT_MASK;
	PIT->MCR &= 0x0u;
	uint32_t period = SystemCoreClock * 15;  // kazdych 15 sekund se zmeni zprava


	PIT->CHANNEL[0].LDVAL = period - 1;
	PIT->CHANNEL[0].TCTRL |= PIT_TCTRL_TIE_MASK;
	PIT->CHANNEL[0].TCTRL |= PIT_TCTRL_TEN_MASK;

	NVIC_SetPriority(PIT0_IRQn, 3);
	NVIC_ClearPendingIRQ(PIT0_IRQn);
	NVIC_EnableIRQ(PIT0_IRQn);

}


/* Variable delay loop */
void delay(int t1, int t2)
{
	int i, j;

	for(i=0; i<t1; i++) {
		for(j=0; j<t2; j++);
	}
}

void process_message()
{
	// maximum length of the message is 30 characters
	if(sizeof(message)/sizeof(char) > MAXMESSAGELENGTH)
	{
		return;
	}

	int bytes_to_copy = 0;
	int i = 0;
	to_display_length = 0;
	FontCharacter char_to_process;

	while(message[i] != '\0')
	{
		switch(message[i])
		{
			case 'a' ... 'z':
				char_to_process = characters[message[i] - 'a'];
				break;
			case 'A' ... 'Z':
				char_to_process = characters[message[i] - 'A'];
				break;
			case '0' ... '9':
				char_to_process = characters[message[i] - 22];
				break;
			case ' ':
				char_to_process = characters[36];
				break;
			case '!':
				char_to_process = characters[37];
				break;
			default:
				i++;
				continue;				// skip unknown characters
				break;
		}

		memcpy(&to_display[to_display_length], char_to_process.columns, char_to_process.width);

		to_display_length += char_to_process.width + 1; // add one row for spaces between characters
		i++;
	}

}


/* Conversion of requested column number into the 4-to-16 decoder control.  */
void column_select(unsigned int col_num)
{
	unsigned i, result, col_sel[4];

	for (i =0; i<4; i++) {
		result = col_num / 2;	  // Whole-number division of the input number
		col_sel[i] = col_num % 2;
		col_num = result;

		switch(i) {

			// Selection signal A0
		    case 0:
				((col_sel[i]) == 0) ? (PTA->PDOR &= ~GPIO_PDOR_PDO( GPIO_PIN(8))) : (PTA->PDOR |= GPIO_PDOR_PDO( GPIO_PIN(8)));
				break;

			// Selection signal A1
			case 1:
				((col_sel[i]) == 0) ? (PTA->PDOR &= ~GPIO_PDOR_PDO( GPIO_PIN(10))) : (PTA->PDOR |= GPIO_PDOR_PDO( GPIO_PIN(10)));
				break;

			// Selection signal A2
			case 2:
				((col_sel[i]) == 0) ? (PTA->PDOR &= ~GPIO_PDOR_PDO( GPIO_PIN(6))) : (PTA->PDOR |= GPIO_PDOR_PDO( GPIO_PIN(6)));
				break;

			// Selection signal A3
			case 3:
				((col_sel[i]) == 0) ? (PTA->PDOR &= ~GPIO_PDOR_PDO( GPIO_PIN(11))) : (PTA->PDOR |= GPIO_PDOR_PDO( GPIO_PIN(11)));
				break;

			// Otherwise nothing to do...
			default:
				break;
		}
	}
}

void row_select(uint8_t row_value)
{
	unsigned i, result;
	uint8_t row_sel[8];
	for (i =0; i<8; i++) {
			result = row_value / 2;	  // Whole-number division of the input number
			row_sel[i] = row_value % 2;
			row_value = result;

			switch(i) {
				// Selection signal R0
			    case 0:
			    	(PORTA->PCR[26] = ( 0|PORT_PCR_MUX(row_sel[i])));
					break;

				// Selection signal R1
				case 1:
					(PORTA->PCR[24] = ( 0|PORT_PCR_MUX(row_sel[i])));
					break;

				// Selection signal R2
				case 2:
					(PORTA->PCR[9] = ( 0|PORT_PCR_MUX(row_sel[i])));
					break;

				// Selection signal R3
				case 3:
					(PORTA->PCR[25] = ( 0|PORT_PCR_MUX(row_sel[i])));
					break;
				// Selection signal R4
				case 4:
					(PORTA->PCR[28] = ( 0|PORT_PCR_MUX(row_sel[i])));
					break;

				// Selection signal R5
				case 5:
					(PORTA->PCR[7] = ( 0|PORT_PCR_MUX(row_sel[i])));
					break;

				// Selection signal R6
				case 6:
					(PORTA->PCR[27] = ( 0|PORT_PCR_MUX(row_sel[i])));
					break;

				// Selection signal R7
				case 7:
					(PORTA->PCR[29] = ( 0|PORT_PCR_MUX(row_sel[i])));
					break;

				// Otherwise nothing to do...
				default:
					break;
			}
	}
}


void PORTE_IRQHandler(void) {
	if (PORTE->ISFR & (1 << 27)) {		// SW4 pressed
			activeMessage = MESSAGE2;
			change = true;

	}
	if (PORTE->ISFR & (1 << 26)) {		// SW5 pressed
		activeMessage = MESSAGE1;
		change = true;
	}
	memset(msg_uart, 0, sizeof(msg_uart));
	msg_uart_length = 0;

	PORTE->ISFR = 1 << 26;
	PORTE->ISFR = 1 << 27;
}

void PIT0_IRQHandler(void) {

	if (activeMessage == MESSAGE1)
	{
		change = true;
		activeMessage = MESSAGE2;
	} else if (activeMessage == MESSAGE2)
	{
		change = true;
		activeMessage = MESSAGE1;
	}
	PIT->CHANNEL[0].TFLG &= PIT_TFLG_TIF_MASK;
	NVIC_ClearPendingIRQ(PIT0_IRQn);
}

char getChar()
{
	char c = UART5->D;
	return c;
}

void receiveChar()
{
	char c = getChar();
	if (msg_uart_length < MAXMESSAGELENGTH )
	{
		msg_uart[msg_uart_length] = getChar();	// add char to the end of uart message
		msg_uart_length++;
		activeMessage = UARTMESSAGE;
		change = true;
	}
}

int main(void)
{
	MCUInit();
	SystemConfig();

	row_select(0);
	PTA->PDOR |= GPIO_PDOR_PDO(0x3F000280); // turning the pixels of a particular row ON

    for (;;) {
    	for (int j = 0; j < WINDOWSIZE - 1; j++)
    	{
    		if (UART5->S1 & UART_S1_RDRF_MASK)
    		{
    			receiveChar();
    		}
    		if (change)		// switch to different message
			{
				memset(message, 0, sizeof(message));
				memset(to_display, 0, sizeof(to_display));

    			if (activeMessage == MESSAGE2)
    			{
    				memcpy(message, message2, strlen(message2));
    			} else if (activeMessage == MESSAGE1)
    			{
    				memcpy(message, default_message, strlen(default_message));
    			} else if (activeMessage == UARTMESSAGE)
    			{
    				memcpy(message, msg_uart, msg_uart_length);
    			}

    			PIT->CHANNEL[0].TCTRL &= ~PIT_TCTRL_TEN_MASK;	// reset timer for changing messages
    			PIT->CHANNEL[0].TCTRL |= PIT_TCTRL_TEN_MASK;

    			change = false;
				process_message();

				window_start_index = -WINDOWSIZE;	// start from beginning of the message
				j = 0;
			}

			for (int i = 0; i < WINDOWSIZE; i++) {	// multiplexing through all the columns

				if ((window_start_index+i < to_display_length - 1) && (window_start_index+i >= 0))
				{
					row_select(to_display[window_start_index+i]);
				} else
				{
					row_select(0);
				}

				column_select(i);
				PTE->PDOR &= ~GPIO_PDOR_PDO( GPIO_PIN(28) );
				delay(tdelay1, tdelay2);						// keep it still for a little while
				PTE->PDDR |= GPIO_PDOR_PDO( GPIO_PIN(28));
			}
    	}

		window_start_index++;
		if (window_start_index == to_display_length + 1)	// got to the end of the message
		{
			window_start_index = -WINDOWSIZE;
		}
    }


    /* Never leave main */
    return 0;
}
