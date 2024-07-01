#include <stdio.h>
#include "board.h"
#include "peripherals.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "MIMXRT1176_cm7.h"
#include "fsl_debug_console.h"
#include "fsl_flexcan.h"

/* Definitions */
#define EXAMPLE_CAN CAN3
#define EXAMPLE_CAN_CLK_FREQ ((CLOCK_GetRootClockFreq(kCLOCK_Root_Can3) / 100000U) * 100000U)
#define RX_MESSAGE_BUFFER_NUM (8)
#define TX_MESSAGE_BUFFER_NUM (9)
/* Select OSC24Mhz as master flexcan clock source */
#define FLEXCAN_CLOCK_SOURCE_SELECT (1U)
/* Clock divider for master flexcan clock source */
#define FLEXCAN_CLOCK_SOURCE_DIVIDER (1U)
/* CAN3 interrupt */
#define EXAMPLE_CAN_IRQn CAN3_IRQn
#define EXAMPLE_CAN_IRQHandler CAN3_IRQHandler
/* Set the baudrate to 500kbps */
#define CAN_BITRATE 500000U

/* Variables */
flexcan_handle_t flexcanHandle;
flexcan_mb_transfer_t txXfer, rxXfer;
flexcan_frame_t txFrame, rxFrame;
volatile bool rxComplete = false;
volatile bool txComplete = false;

/* Initialization function */
void EXAMPLE_CAN_IRQHandler(void);
void CAN_Init(void);
void FLEXCAN_UserCallback(CAN_Type *base, flexcan_handle_t *handle,
		status_t status, uint32_t result, void *userData);

/*The FLEXCAN_UserCallback function serves as a callback to handle various events 
such as message reception, transmission completion, and error conditions.*/
void FLEXCAN_UserCallback(CAN_Type *base, flexcan_handle_t *handle,
		status_t status, uint32_t result, void *userData) 
{
	switch (status) //It checks the status code reported by the FlexCAN module
	{
	case kStatus_FLEXCAN_RxIdle: // This flag chekcs the  receive operation has completed successfully
		rxComplete = true;
		break;
	case kStatus_FLEXCAN_TxIdle:// This flag chekcs the  Transmit operation has completed successfully
		txComplete = true;
		break;
	case kStatus_FLEXCAN_ErrorStatus://This flag indicating the specific CAN error status (result) encountered during CAN communication
		PRINTF("CAN Error Status: 0x%x\r\n", result);
		break;
	default:
		break;
	}
}

/*The CAN_Init function is responsible for initializing the CAN module, configuring its settings,
 and preparing it for communication on the CAN bus.*/
void CAN_Init(void) 
{
	flexcan_config_t flexcanConfig;
	flexcan_rx_mb_config_t mbConfig;
	flexcan_timing_config_t timingConfig;

	/* Get FlexCAN module default configuration */
	FLEXCAN_GetDefaultConfig(&flexcanConfig);

	/* Set the appropriate clock source */
	flexcanConfig.clkSrc = kFLEXCAN_ClkSrc0;

	// Calculate timing parameters for the desired bitrate
	if (FLEXCAN_CalculateImprovedTimingValues(EXAMPLE_CAN, CAN_BITRATE,
			EXAMPLE_CAN_CLK_FREQ, &timingConfig)) {
		flexcanConfig.timingConfig = timingConfig;
	} else {
		PRINTF("Error in calculating timing parameters\n");
		return;
	}

	/* Initialize FlexCAN module */
	FLEXCAN_Init(EXAMPLE_CAN, &flexcanConfig, EXAMPLE_CAN_CLK_FREQ);

	/* Setup Tx Message Buffer */
	FLEXCAN_SetTxMbConfig(EXAMPLE_CAN, TX_MESSAGE_BUFFER_NUM, true);

	/* Setup Rx Message Buffer */
	mbConfig.format = kFLEXCAN_FrameFormatStandard;
	mbConfig.type = kFLEXCAN_FrameTypeData;
	mbConfig.id = FLEXCAN_ID_STD(0x45);
	FLEXCAN_SetRxMbConfig(EXAMPLE_CAN, RX_MESSAGE_BUFFER_NUM, &mbConfig, true);

	/* Create FlexCAN handle structure and set callback function */
	FLEXCAN_TransferCreateHandle(EXAMPLE_CAN, &flexcanHandle,
			FLEXCAN_UserCallback, NULL);

	// Enable interrupts for Rx and Tx Message Buffers
	FLEXCAN_EnableMbInterrupts(EXAMPLE_CAN, 1 << RX_MESSAGE_BUFFER_NUM);
	FLEXCAN_EnableMbInterrupts(EXAMPLE_CAN, 1 << TX_MESSAGE_BUFFER_NUM);
	EnableIRQ(EXAMPLE_CAN_IRQn);
}

/* Main function */
int main(void) 
{
	/* Init board hardware */
	BOARD_InitBootPins();
	BOARD_InitBootClocks();
	BOARD_InitBootPeripherals();
#ifndef BOARD_INIT_DEBUG_CONSOLE_PERIPHERAL
	/* Init FSL debug console */
	BOARD_InitDebugConsole();
#endif

	/* Print CAN example start */
	PRINTF("CAN example start...\r\n");

	/* Initialize CAN module */
	CAN_Init();
	flexcan_frame_format_t frameFormat = kFLEXCAN_FrameFormatStandard;
	flexcan_frame_type_t frameType = kFLEXCAN_FrameTypeData;

	// Prepare Tx Frame
	txFrame.format = kFLEXCAN_FrameFormatStandard;
	txFrame.type = kFLEXCAN_FrameTypeData;
	txFrame.length = 8; // Data length

	txXfer.frame = &txFrame;
	txXfer.mbIdx = TX_MESSAGE_BUFFER_NUM;

	// Prepare Rx Frame
	rxXfer.frame = &rxFrame;
	rxXfer.mbIdx = RX_MESSAGE_BUFFER_NUM;

	while (1) 
	{
		// Check if there's a received message
		if (rxComplete) 
		{
			rxComplete = false;

			PRINTF("Received message: ID = 0x%x, Data: 0x%x 0x%x\r\n",
					(rxFrame.id >> CAN_ID_STD_SHIFT), rxFrame.dataWord0,
					rxFrame.dataWord1);

			// Prepare and send a response message
			txFrame.id = rxFrame.id;  // Echo received ID
			txFrame.dataWord0 = rxFrame.dataWord0;  // Example data
			txFrame.dataWord1 = rxFrame.dataWord1;  // Example data

			FLEXCAN_TransferSendNonBlocking(EXAMPLE_CAN, &flexcanHandle,
					&txXfer);
		}

		// Check if a transmission is complete
		if (txComplete) 
		{
			txComplete = false;
			PRINTF("Transmission complete\r\n");

		}

		// Receive another message
		FLEXCAN_TransferReceiveNonBlocking(EXAMPLE_CAN, &flexcanHandle,
				&rxXfer);

	}

	return 0;
}
