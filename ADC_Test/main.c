#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/debug.h"
#include "driverlib/sysctl.h"
#include "driverlib/adc.h"
#include "driverlib/uart.h"
#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/interrupt.h"
#include "inc/hw_ints.h"
#include "driverlib/timer.h"

unsigned long ulADC0Value[4];
volatile unsigned char output_data[30] = "Temperature is: 000 Celsius\n\r";
volatile unsigned long ulTempAvg;
volatile unsigned long ulTempValueC;
volatile unsigned long ulTempValueF;
volatile uint8_t count = 0;
volatile uint32_t led_status = GPIO_PIN_1|GPIO_PIN_2;

#ifdef DEBUG
void__error__(char *pcFilename, unsigned long ulLine)
{
}
#endif

//void UARTIntHandler(void)
//{
//	uint32_t ui32Status;
//	ui32Status = UARTIntStatus(UART0_BASE, true); //get interrupt status
//	UARTIntClear(UART0_BASE, ui32Status); //clear the asserted interrupts
//	while(UARTCharsAvail(UART0_BASE)) //loop while there are chars
//	{
//		UARTCharPutNonBlocking(UART0_BASE,UARTCharGetNonBlocking(UART0_BASE)); //echo character
//		GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2, GPIO_PIN_2); //blink LED
//		SysCtlDelay(SysCtlClockGet() / (1000 * 3)); //delay ~1 msec
//		GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2, 0); //turn off LED
//	}
//}

// Send a string to the UART.
void UARTSend(const uint8_t *pui8Buffer, uint32_t ui32Count)
{
    //
    // Loop while there are more characters to send.
    //
    while(ui32Count--)
    {
        //
        // Write the next character to the UART.
        //
        ROM_UARTCharPutNonBlocking(UART0_BASE, *pui8Buffer++);
    }
}

void UARTPutString(uint32_t UART_Base, const char *s) {
	while(*s)
		{
			UARTCharPut(UART_Base, *s++);
		}
}

void UARTPutNumber(uint32_t UART_Base, long Num)
{
	unsigned long temp = 1;
	long NumTemp;
	NumTemp = Num;
	if (Num == 0)
		UARTCharPut(UART_Base, 48);
	else {
		if (Num < 0)
		{
			UARTCharPut(UART_Base, '-');
			Num *= -1;
		}
		while (NumTemp) {
			NumTemp /= 10;
			temp *= 10;
		}
		temp /= 10;
		while (temp) {
			UARTCharPut(UART_Base,(Num / temp) % 10 + 48);
			temp /= 10;
		}
	}
}
void Timer0IntHandle(void){
	//Clear Int Flag
	TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
	if (count == 1000){
		count = 0;
		led_status ^= GPIO_PIN_1;
	} else {
		count += 1;
	}

	GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1|GPIO_PIN_2, led_status);
}

int main(void) {
	static uint32_t ulPeriod;

	// Set system clock
	SysCtlClockSet(SYSCTL_SYSDIV_5|SYSCTL_USE_PLL|SYSCTL_OSC_MAIN|SYSCTL_XTAL_16MHZ);

	// Enable Peripheral GPIO port F, ADC0, UART0 and Timer0
	SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
	SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);

    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);

    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF); //enable GPIO port for LED
    GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_2); //enable pin for LED PF2

	// Set up Timer 0
	ulPeriod = SysCtlClockGet()/1000;
	TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);
	TimerLoadSet(TIMER0_BASE, TIMER_A, ulPeriod);
	TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
	IntEnable(INT_TIMER0A);
	IntMasterEnable();
	TimerEnable(TIMER0_BASE, TIMER_A);

	// Set up ADC0
	SysCtlADCSpeedSet(SYSCTL_ADCSPEED_250KSPS);
	ADCSequenceDisable(ADC0_BASE, 1);
	ADCSequenceConfigure(ADC0_BASE, 1, ADC_TRIGGER_PROCESSOR, 0);
	ADCSequenceStepConfigure(ADC0_BASE, 1, 0, ADC_CTL_TS);
	ADCSequenceStepConfigure(ADC0_BASE, 1, 1, ADC_CTL_TS);
	ADCSequenceStepConfigure(ADC0_BASE, 1, 2, ADC_CTL_TS);
	ADCSequenceStepConfigure(ADC0_BASE, 1, 3, ADC_CTL_TS | ADC_CTL_IE|ADC_CTL_END);
	ADCSequenceEnable(ADC0_BASE, 1);

	UARTConfigSetExpClk(UART0_BASE, SysCtlClockGet(), 115200, (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |UART_CONFIG_PAR_NONE));
    IntMasterEnable(); //enable processor interrupts
    IntEnable(INT_UART0); //enable the UART interrupt
    UARTIntEnable(UART0_BASE, UART_INT_RX | UART_INT_RT); //only enable RX and TX interrupts

	while(1){
		ADCIntClear(ADC0_BASE, 1);
		ADCProcessorTrigger(ADC0_BASE, 1);
		if(!ADCIntStatus(ADC0_BASE, 1, false)){
		}

		ADCSequenceDataGet(ADC0_BASE, 1, ulADC0Value);
		ulTempAvg = (ulADC0Value[0] + ulADC0Value[1] + ulADC0Value[2] + ulADC0Value[3]+ 2)/4;
		ulTempValueC = (1475 - ((2475 * ulTempAvg)) / 4096)/10;
		ulTempValueF = ((ulTempValueC * 9) + 160) / 5;

//        a[3] = ulADC0Value[0]%10 + 48;
 //       ulADC0Value[0] = ulADC0Value[0]/10 ;
//        a[2] = ulADC0Value[0]%10 + 48;
 //       ulADC0Value[0] = ulADC0Value[0]/10 ;
 //     	a[1] = ulADC0Value[0]%10 + 48;
 //     	a[0] = ulADC0Value[0]/10 + 48;
//
      	printf("Temperature is: %lu Celsius\n\r", ulTempValueC,output_data);

      	SysCtlDelay(SysCtlClockGet()/3); //delay ~1 second
		for(i=0;i<30;i++){		// Loop to print out data string
			UARTCharPut(UART0_BASE, output_data[i]);
		}

	}
}
