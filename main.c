/*
 * MSP432 Simple Inverter Controller
 * Toggles switches in a particular order to generate a
 * 60Hz waveform on the output of an Inverter
 *
 * Written by Rebel
 */
#include "msp.h"

// !TODO: Remove this definition once the header file is updated this def.
#define CS_KEY 0x695A
// !TODO: Remove this definition once the header file is updated this def.
#define FLCTL_BANK0_RDCTL_WAIT__2    (2 << 12)
#define FLCTL_BANK1_RDCTL_WAIT__2    (2 << 12)

#define m_freq 48000000
#define dead_time_switching 4

/*
 * Initialization Functions
 */
void setup_clock();
void setup_TimerA1(uint32_t);
void setup_Ports();

/*
 * Utility Functions
 */
uint32_t pwmFreqTicksCalc(uint32_t, uint32_t);
void error();

/*
 * ISR Functions
 */
void TimerA1_ISR();

void main(void)
{
	
    WDTCTL = WDTPW | WDTHOLD;           // Stop watchdog timer
	
    // Initialize Clock
    setup_clock();
    // Setup the interupts for a 120Hz
	uint32_t ticks_mod = pwmFreqTicksCalc(120, m_freq/8);

	// Set Up the 60Hz Stuff
	setup_Ports();
	setup_TimerA1(ticks_mod);

	__enable_interrupts();

	while(1);
}

void setup_Ports(){
	//	Signals on port 4 are used for Inverter
	// Output Signals + initialize to low
	P4DIR |= BIT0 + BIT1 + BIT2 + BIT3;
	P4OUT &= ~(BIT0 + BIT1 + BIT2 + BIT3);

	// Used to Indicate if negative load is present
	P1DIR |= BIT0;
	P1OUT &= ~BIT0;
}

/*
 * TimerA1 Setup - Overflow Setup + Increment Setup
 * Init timerA1 to generate an interupt every 60Hz
 * As well as another Register designed control sine step
 */
void setup_TimerA1(uint32_t ticks){
	// Generic Error if ticks is Greater than 16bit
	if(ticks > 65000){
		error();
	}

	// Set up the CCR0
	TA1CCR0 = ticks;

	// Enable Interrupts
	NVIC_ISER0 = 1 << ((INT_TA1_N - 16) & 31);	//	Allow Enabling Interrupts

	//	Start the timer! + enable interrupts
	TA1CTL = TASSEL__SMCLK + MC__UP + ID__8 + TAIE;
}

/*
 * Sets up the System Clock to MCLK
 */
void setup_clock() {
	uint32_t currentPowerState;

	// NOTE: This assumes the default power state is AM0_LDO.

	/* Step 1: Transition to VCORE Level 1: AM0_LDO --> AM1_LDO */

	/* Get current power state, if it's not AM0_LDO, error out */
	currentPowerState = PCMCTL0 & CPM_M;
	if (currentPowerState != CPM_0)
		error();

	while ((PCMCTL1 & PMR_BUSY));
	PCMCTL0 = CS_KEY<<16 | AMR_1;
	while ((PCMCTL1 & PMR_BUSY));
	if (PCMIFG & AM_INVALID_TR_IFG)
		error();                            // Error if transition was not successful
	if ((PCMCTL0 & CPM_M) != CPM_1)
		error();                            // Error if device is not in AM1_LDO mode

	/* Step 2: Configure Flash wait-state to 2 for both banks 0 & 1 */
	FLCTL_BANK0_RDCTL = FLCTL_BANK0_RDCTL & ~FLCTL_BANK0_RDCTL_WAIT_M | FLCTL_BANK0_RDCTL_WAIT_2;
	FLCTL_BANK1_RDCTL = FLCTL_BANK0_RDCTL & ~FLCTL_BANK1_RDCTL_WAIT_M | FLCTL_BANK1_RDCTL_WAIT_2;

	/* Step 3: Configure DCO to 48MHz, ensure MCLK uses DCO as source*/
	CSKEY = CS_KEY;                        // Unlock CS module for register access
	CSCTL0 = 0;                            // Reset tuning parameters
	CSCTL0 = DCORSEL_5;                    // Set DCO to 48MHz
	/* Select MCLK = DCO, no divider */
	CSCTL1 = CSCTL1 & ~(SELM_M | DIVM_M) | SELM_3;
	CSKEY = 0;                             // Lock CS module from unintended accesses
}

/*
 * Calculates the number ticks required to generate a desired
 * number of ticks needed to achieve frequency
 * Note assumes that Operating Frequency is greater than Target
 */
uint32_t pwmFreqTicksCalc(uint32_t target_freq, uint32_t operating_freq){
	double period_desired = (double)1/target_freq;
	double period_operating = (double)1/operating_freq;
	return period_desired/period_operating;
}

void error(void)
{
    volatile uint32_t i;
    P1DIR |= BIT0;
    while (1)
    {
        P1OUT ^= BIT0;
        __delay_cycles(3100000);
    }
}

/*
 * Note: On PORT4 the following is the switch order
 * 1   0
 *   L
 * 3   2
 * Where the load would be in the middle
 * 1 & 0 are high
 * 3 and 2 are grounded
 */
void TimerA1_ISR(){
	//	Control the Switches to generate 60Hz Waveform
	//	Note: LED means negative state

	// Clear the interrupt Flag
	TA1IV = 0;

	//	Deadtime - Prevents Shorts
	P4OUT = 0x00;	// Turn off all Switches
	__delay_cycles(dead_time_switching);

	//	Current State
	if(P1OUT & BIT0)
		P4OUT = 0x09;	// Negative Load
	else
		P4OUT = 0x06;	//	Postive Load

	//	Toggle the LED - indiate next state
	P1OUT ^= BIT0;

}
