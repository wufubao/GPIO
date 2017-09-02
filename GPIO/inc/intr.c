/*****************************************************************************/
/**
* @file main.c
*
* This file contains a design example using the GPIO driver (XGpio) in an
* interrupt driven mode of operation. This example does assume that there is
* an interrupt controller in the hardware system and the GPIO device is
* connected to the interrupt controller.
*
* This file is used in the Peripheral Tests Application in SDK to include a
* simplified test for gpio interrupts.

* The buttons and LEDs are on 2 separate channels of the GPIO so that interrupts
* are not caused when the LEDs are turned on and off.
*
*
******************************************************************************/
#include "platform.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "xscugic.h"
#include "xgpio.h"
#define INTC_DEVICE_ID			XPAR_SCUGIC_SINGLE_DEVICE_ID
#define GPIO_DEVICE_ID			XPAR_GPIO_0_DEVICE_ID
#define INTC_GPIO_INTERRUPT_ID	XPAR_FABRIC_AXI_GPIO_0_IP2INTC_IRPT_INTR
#define INTC_HANDLER			XScuGic_InterruptHandler
#define INTC					XScuGic
#define GPIO_CHANNEL1			1
static u16 GlobalIntrMask;
XGpio Gpio;
INTC Intc;
static volatile u32 IntrFlag;

void GpioHandler(void *CallBackRef);
int GpioIntrExample(INTC *IntcInstancePtr, XGpio *GpioInstancePtr,
			u16 DeviceId, u16 IntrId,
			u16 IntrMask);

int GpioSetupIntrSystem(INTC *IntcInstancePtr, XGpio *GpioInstancePtr,
			u16 DeviceId, u16 IntrId, u16 IntrMask);

void GpioDisableIntr(INTC *IntcInstancePtr, XGpio *GpioInstancePtr,
			u16 IntrId, u16 IntrMask);

int main()
{
    init_platform();
    u32 Status;
    XGpio GPIO_SW;
    xil_printf("Enter the main func\r\n");
	Status = XGpio_Initialize(&GPIO_SW, XPAR_AXI_GPIO_0_DEVICE_ID);
	if (Status != XST_SUCCESS) return XST_FAILURE;
	xil_printf(" Press button to Generate Interrupt\r\n");
	Status = GpioIntrExample(&Intc, &GPIO_SW,
				   GPIO_DEVICE_ID,
				   INTC_GPIO_INTERRUPT_ID,
				   GPIO_CHANNEL1);
	if (Status == XST_SUCCESS)
		xil_printf("btn been pressed!\r\n");
    cleanup_platform();
    return 0;
}

/******************************************************************************/
/**
*
* This is the entry function from the TestAppGen tool generated application
* which tests the interrupts when enabled in the GPIO
*
* @param	IntcInstancePtr is a reference to the Interrupt Controller
*		driver Instance
* @param	InstancePtr is a reference to the GPIO driver Instance
* @param	DeviceId is the XPAR_<GPIO_instance>_DEVICE_ID value from
*		xparameters.h
* @param	IntrId is XPAR_<INTC_instance>_<GPIO_instance>_IP2INTC_IRPT_INTR
*		value from xparameters.h
* @param	IntrMask is the GPIO channel mask
*
* @return
*		- XST_SUCCESS if the Test is successful
*		- XST_FAILURE if the test is not successful
*
* @note		None.
*
******************************************************************************/
int GpioIntrExample(INTC *IntcInstancePtr, XGpio* InstancePtr, u16 DeviceId,
			u16 IntrId, u16 IntrMask)
{
	int Status;

	/* Initialize the GPIO driver. If an error occurs then exit */
	Status = XGpio_Initialize(InstancePtr, DeviceId);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	Status = GpioSetupIntrSystem(IntcInstancePtr, InstancePtr, DeviceId,
					IntrId, IntrMask);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	IntrFlag = 0;

		while (1) {
		/*
		 * If the interrupt occurred which is indicated by the global
		 * variable which is set in the device driver handler, then
		 * stop waiting
		 */
		if (IntrFlag) {
			break;
		}
		}

	GpioDisableIntr(IntcInstancePtr, InstancePtr, IntrId, IntrMask);
	return XST_SUCCESS;
}

int GpioSetupIntrSystem(INTC *IntcInstancePtr, XGpio *GpioInstancePtr,
	u16 DeviceId, u16 IntrId, u16 IntrMask){

	int Result;
	GlobalIntrMask = IntrMask;
	XScuGic_Config *IntcConfig;
	/*
	 * Initialize the interrupt controller driver so that it is ready to
	 * use.
	 */
	IntcConfig = XScuGic_LookupConfig(INTC_DEVICE_ID);
	if (NULL == IntcConfig) {
		return XST_FAILURE;
	}

	Result = XScuGic_CfgInitialize(IntcInstancePtr, IntcConfig,
					IntcConfig->CpuBaseAddress);
	if (Result != XST_SUCCESS) {
		return XST_FAILURE;
	}
	XScuGic_SetPriorityTriggerType(IntcInstancePtr, IntrId,
					0xA0, 0x3);

	/*
	 * Connect the interrupt handler that will be called when an
	 * interrupt occurs for the device.
	 */
	Result = XScuGic_Connect(IntcInstancePtr, IntrId,
				 (Xil_ExceptionHandler)GpioHandler, GpioInstancePtr);
	if (Result != XST_SUCCESS) {
		return Result;
	}
	/* Enable the interrupt for the GPIO device.*/
	XScuGic_Enable(IntcInstancePtr, IntrId);
	/*
	 * Enable the GPIO channel interrupts so that push button can be
	 * detected and enable interrupts for the GPIO device
	 */
	XGpio_InterruptEnable(GpioInstancePtr, IntrMask);
	XGpio_InterruptGlobalEnable(GpioInstancePtr);
		/*
	 * Initialize the exception table and register the interrupt
	 * controller handler with the exception table
	 */
	Xil_ExceptionInit();

	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
			 (Xil_ExceptionHandler)INTC_HANDLER, IntcInstancePtr);

	/* Enable non-critical exceptions */
	Xil_ExceptionEnable();

	return XST_SUCCESS;

}

/******************************************************************************/
/**
*
* This is the interrupt handler routine for the GPIO for this example.
*
* @param	CallbackRef is the Callback reference for the handler.
*
* @return	None.
*
* @note		None.
*
******************************************************************************/
void GpioHandler(void *CallbackRef)
{
	XGpio *GpioPtr = (XGpio *)CallbackRef;
	xil_printf("impl the intr func!\r\n");
	IntrFlag = 1;

	/* Clear the Interrupt */
	XGpio_InterruptClear(GpioPtr, GlobalIntrMask);

}


/******************************************************************************/
/**
*
* This function disables the interrupts for the GPIO
*
* @param	IntcInstancePtr is a pointer to the Interrupt Controller
*		driver Instance
* @param	InstancePtr is a pointer to the GPIO driver Instance
* @param	IntrId is XPAR_<INTC_instance>_<GPIO_instance>_VEC
*		value from xparameters.h
* @param	IntrMask is the GPIO channel mask
*
* @return	None
*
* @note		None.
*
******************************************************************************/
void GpioDisableIntr(INTC *IntcInstancePtr, XGpio *InstancePtr,
			u16 IntrId, u16 IntrMask)
{
	XGpio_InterruptDisable(InstancePtr, IntrMask);
	XScuGic_Disable(IntcInstancePtr, IntrId);
	XScuGic_Disconnect(IntcInstancePtr, IntrId);
}
