#include "platform.h"
#include "xil_printf.h"
#include "xaxidma.h"
#include "xparameters.h"
#include "xil_mmu.h"
#include "xil_exception.h"
#include "xdebug.h"

#define DMA_DEV_ID		XPAR_AXIDMA_0_DEVICE_ID
#define MEM_BASE_ADDR		0x01000000
#define RX_BD_SPACE_BASE	(MEM_BASE_ADDR)
#define RX_BD_SPACE_HIGH	(MEM_BASE_ADDR + 0x0000FFFF)
#define TX_BD_SPACE_BASE	(MEM_BASE_ADDR + 0x00010000)
#define TX_BD_SPACE_HIGH	(MEM_BASE_ADDR + 0x0001FFFF)
#define TX_BUFFER_BASE		(MEM_BASE_ADDR + 0x00100000)
#define RX_BUFFER_BASE		(MEM_BASE_ADDR + 0x00300000)
#define RX_BUFFER_HIGH		(MEM_BASE_ADDR + 0x004FFFFF)

#define MAX_PKT_LEN		0x20
#define MARK_UNCACHEABLE        0x701

#define NUM_BD_TO_TRANSFER 10

u32 *Packet = (u32 *) TX_BUFFER_BASE;
int RxSetup(XAxiDma * AxiDmaInstPtr);
int TxSetup(XAxiDma * AxiDmaInstPtr);
int XaxiDma_Initialize(XAxiDma * InstancePtr, u32 DeviceId);
int SendPacket(XAxiDma * AxiDmaInstPtr);
int main(void){
    init_platform();
	int Status;
	XAxiDma AxiDma;

	xil_printf("\r\n--- Entering main() --- \r\n");

	Xil_SetTlbAttributes(TX_BD_SPACE_BASE, MARK_UNCACHEABLE);
	Xil_SetTlbAttributes(RX_BD_SPACE_BASE, MARK_UNCACHEABLE);

	Status = XaxiDma_Initialize(&AxiDma, DMA_DEV_ID);
	if (Status != XST_SUCCESS) {
		xil_printf("Initialization failed %d\r\n");
		return XST_FAILURE;
	}

	if(!XAxiDma_HasSg(&AxiDma)) {
		xil_printf("Device configured as Simple mode \r\n");
		return XST_FAILURE;
	}

	Status = RxSetup(&AxiDma);
	if (Status != XST_SUCCESS) {
		xil_printf("RxSetup failed \r\n");
		return XST_FAILURE;
	}

	Status = TxSetup(&AxiDma);
	if (Status != XST_SUCCESS) {
		xil_printf("TxSetup failed \r\n");
		return XST_FAILURE;
	}

	/* Send a packet */
	Status = SendPacket(&AxiDma);
	if (Status != XST_SUCCESS) {
		xil_printf("SendPacket failed \r\n");
		return XST_FAILURE;
	}
//	Status = DmaFree(&AxiDma);
//	if (Status != XST_SUCCESS) {
//		xil_printf("DmaFree failed \r\n");
//		return XST_FAILURE;
//	}
//	xil_printf("Result of \r\n");Xil_In16();
	xil_printf("\r\n--- Exiting main() --- \r\n");
    cleanup_platform();
    return 0;
}

/*****************************************************************************/
/**
*
* Initialize the DMA configuration.
*
* @param	AxiDmaInstPtr is the pointer to the instance of the DMA engine.
*
* @param	DeviceId is the unique device ID of the device to lookup for.
*
* @return	XST_SUCCESS if the setup is successful, XST_FAILURE otherwise.
*
* @note		None.
*
******************************************************************************/
int XaxiDma_Initialize(XAxiDma * InstancePtr, u32 DeviceId){
	u32 Status;
	XAxiDma_Config *Config;
	Config = XAxiDma_LookupConfig(DeviceId);
	if (!Config) {
		xil_printf("No config found for %d\r\n", DeviceId);
		return XST_FAILURE;
	}
	/* Initialize DMA engine */
	Status = XAxiDma_CfgInitialize(InstancePtr, Config);
	if (Status != XST_SUCCESS) {
		xil_printf("Initialization failed %d\r\n", Status);
		return XST_FAILURE;
	}
	return XST_SUCCESS;
}

/*****************************************************************************/
/**
*
* This function sets up RX channel of the DMA engine to be ready for packet
* reception
*
* @param	AxiDmaInstPtr is the pointer to the instance of the DMA engine.
*
* @return	XST_SUCCESS if the setup is successful, XST_FAILURE otherwise.
*
* @note		None.
*
******************************************************************************/
int RxSetup(XAxiDma * AxiDmaInstPtr){
	XAxiDma_BdRing *RxRingPtr;
	int Delay = 0;
	int Coalesce = 1;
	int Status;
	XAxiDma_Bd BdTemplate;
	XAxiDma_Bd *BdPtr;
	XAxiDma_Bd *BdCurPtr;
	u32 BdCount;
	u32 FreeBdCount;
	UINTPTR RxBufferPtr;
	int Index;
	RxRingPtr = XAxiDma_GetRxRing(AxiDmaInstPtr);

	/* Disable all RX interrupts before RxBD space setup */

	XAxiDma_BdRingIntDisable(RxRingPtr, XAXIDMA_IRQ_ALL_MASK);

	/* Set delay and coalescing */
	XAxiDma_BdRingSetCoalesce(RxRingPtr, Coalesce, Delay);

	/* Setup Rx BD space */
	BdCount = XAxiDma_BdRingCntCalc(XAXIDMA_BD_MINIMUM_ALIGNMENT,
				RX_BD_SPACE_HIGH - RX_BD_SPACE_BASE + 1);

	Status = XAxiDma_BdRingCreate(RxRingPtr, RX_BD_SPACE_BASE,
				RX_BD_SPACE_BASE,
				XAXIDMA_BD_MINIMUM_ALIGNMENT, BdCount);
	if (Status != XST_SUCCESS) {
		xil_printf("RX create BD ring failed %d\r\n", Status);
		return XST_FAILURE;
	}

	/*
	 * Setup an all-zero BD as the template for the Rx channel.
	 */
	XAxiDma_BdClear(&BdTemplate);
	Status = XAxiDma_BdRingClone(RxRingPtr, &BdTemplate);
	if (Status != XST_SUCCESS) {
		xil_printf("RX clone BD failed %d\r\n");
		return XST_FAILURE;
	}

	/* Attach buffers to RxBD ring so we are ready to receive packets */
	FreeBdCount = XAxiDma_BdRingGetFreeCnt(RxRingPtr);
//	xil_printf("FreeBdCount is %d\r\n",FreeBdCount);
	// BdPtr is the first BD to the RxRing
	Status = XAxiDma_BdRingAlloc(RxRingPtr, NUM_BD_TO_TRANSFER, &BdPtr);
	if (Status != XST_SUCCESS) {
		xil_printf("RX alloc BD failed %d\r\n");
		return XST_FAILURE;
	}

	BdCurPtr = BdPtr;
	RxBufferPtr = RX_BUFFER_BASE;
	for (Index = 0; Index < NUM_BD_TO_TRANSFER; Index++) {
		Status = XAxiDma_BdSetBufAddr(BdCurPtr, RxBufferPtr);
		if (Status != XST_SUCCESS) {
			xil_printf("Set buffer addr %x on BD %x failed %d\r\n",
			    (unsigned int)RxBufferPtr,
			    (UINTPTR)BdCurPtr, Status);
			return XST_FAILURE;
		}
		Status = XAxiDma_BdSetLength(BdCurPtr, MAX_PKT_LEN,
				RxRingPtr->MaxTransferLen);
		if (Status != XST_SUCCESS) {
			xil_printf("Rx set length %d on BD %x failed %d\r\n",
			    MAX_PKT_LEN, (UINTPTR)BdCurPtr, Status);
			return XST_FAILURE;
		}
		if(Index == (NUM_BD_TO_TRANSFER - 1)) {
			/*Point the next pointer back to the first BD so we cycle*/
			XAxiDma_BdWrite((BdCurPtr), XAXIDMA_BD_NDESC_OFFSET, (int)BdPtr);
		}
		/* Receive BDs do not need to set anything for the control
		 * The hardware will set the SOF/EOF bits per stream status
		 */
		XAxiDma_BdSetCtrl(BdCurPtr, 0);
		XAxiDma_BdSetId(BdCurPtr, RxBufferPtr);

		RxBufferPtr += MAX_PKT_LEN;
//		xil_printf("RxBufferPtr %16x\r\n", RxBufferPtr);
		BdCurPtr = (XAxiDma_Bd *)XAxiDma_BdRingNext(RxRingPtr, BdCurPtr);
	}

	/* Clear the receive buffer, so we can verify data
	 */
	memset((void *)RX_BUFFER_BASE, 0, MAX_PKT_LEN);

	Status = XAxiDma_BdRingToHw(RxRingPtr, NUM_BD_TO_TRANSFER,
						BdPtr);
	if (Status != XST_SUCCESS) {
		xil_printf("RX submit hw failed %d\r\n", Status);
		return XST_FAILURE;
	}
	Status = XAxiDma_SelectCyclicMode(AxiDmaInstPtr, XAXIDMA_DEVICE_TO_DMA, TRUE);
	if (Status != XST_SUCCESS)
	{
		xil_printf("Failed to set Cyclic mode on TX\r\n");
		return XST_FAILURE;
	}

	/* Start RX DMA channel */
	Status = XAxiDma_BdRingStart(RxRingPtr);
	if (Status != XST_SUCCESS) {
		xil_printf("RX start hw failed %d\r\n", Status);
		return XST_FAILURE;
	}
	Xil_Out32((XPAR_AXIDMA_0_BASEADDR + XAXIDMA_RX_OFFSET + XAXIDMA_TDESC_OFFSET), 0x140);
	return XST_SUCCESS;
}
/*****************************************************************************/
/**
*
* This function sets up the TX channel of a DMA engine to be ready for packet
* transmission
*
* @param	AxiDmaInstPtr is the instance pointer to the DMA engine.
*
* @return	XST_SUCCESS if the setup is successful, XST_FAILURE otherwise.
*
* @note		None.
*
******************************************************************************/
int TxSetup(XAxiDma * AxiDmaInstPtr){
	XAxiDma_BdRing *TxRingPtr;
	XAxiDma_Bd BdTemplate;
	int Delay = 0;
	int Coalesce = 1;
	int Status;
	u32 BdCount;

	TxRingPtr = XAxiDma_GetTxRing(AxiDmaInstPtr);

	/* Disable all TX interrupts before TxBD space setup */
	XAxiDma_BdRingIntDisable(TxRingPtr, XAXIDMA_IRQ_ALL_MASK);

	/* Set TX delay and coalesce */
	XAxiDma_BdRingSetCoalesce(TxRingPtr, Coalesce, Delay);

	/* Setup TxBD space  */
	BdCount = XAxiDma_BdRingCntCalc(XAXIDMA_BD_MINIMUM_ALIGNMENT,
				TX_BD_SPACE_HIGH - TX_BD_SPACE_BASE + 1);

	Status = XAxiDma_BdRingCreate(TxRingPtr, TX_BD_SPACE_BASE,
				TX_BD_SPACE_BASE,
				XAXIDMA_BD_MINIMUM_ALIGNMENT, BdCount);
	if (Status != XST_SUCCESS) {
		xil_printf("failed create BD ring in txsetup\r\n");
		return XST_FAILURE;
	}

	/*
	 * We create an all-zero BD as the template.
	 */
	XAxiDma_BdClear(&BdTemplate);

	Status = XAxiDma_BdRingClone(TxRingPtr, &BdTemplate);
	if (Status != XST_SUCCESS) {
		xil_printf("failed bdring clone in txsetup %d\r\n", Status);
		return XST_FAILURE;
	}
	// TODO: add Setup all BD using XAxiDma_BdRingToHw(TxRingPtr, NUMBER_OF_BDS_TO_TRANSFER, BdPtr);
	return XST_SUCCESS;
}

/*****************************************************************************/
/**
*
* This function transmits one packet blockingly through the DMA engine.
*
* @param	AxiDmaInstPtr points to the DMA engine instance
*
* @return	- XST_SUCCESS if the DMA accepts the packet successfully,
*		- XST_FAILURE otherwise.
*
* @note     None.
*
******************************************************************************/
int SendPacket(XAxiDma * AxiDmaInstPtr){
	XAxiDma_BdRing *TxRingPtr;
	XAxiDma_Bd *BdPtr, *BdCurPtr;
	int Status;
	int Index;
	u8 *TxPacket;
	u8 Value;
	u32 BufferAddr;
	TxRingPtr = XAxiDma_GetTxRing(AxiDmaInstPtr);
	if ((TxRingPtr->MaxTransferLen) < NUM_BD_TO_TRANSFER) {
		xil_printf("Invalid total per packet transfer length for the "
			"packet %d/%d\r\n",
			NUM_BD_TO_TRANSFER,TxRingPtr->MaxTransferLen);
		return XST_INVALID_PARAM;
	}
	TxPacket = (u8 *) Packet;
	Value = 0xC;
	for(Index = 0; Index < MAX_PKT_LEN * NUM_BD_TO_TRANSFER;
								Index ++) {
		TxPacket[Index] = Value;
		Value = (Value + 1) & 0xFF;
	}
	/* Flush the SrcBuffer before the DMA transfer, in case the Data Cache
	 * is enabled
	 */
	Xil_DCacheFlushRange((UINTPTR)TxPacket, MAX_PKT_LEN);
#ifdef __aarch64__
	Xil_DCacheFlushRange((UINTPTR)RX_BUFFER_BASE, MAX_PKT_LEN);
	// XXX flush length should be total length of the TX part, but it seems working well.
#endif

	/* Allocate a BD */
	Status = XAxiDma_BdRingAlloc(TxRingPtr, NUM_BD_TO_TRANSFER, &BdPtr);
	if (Status != XST_SUCCESS) {
		xil_printf("TX alloc BD failed\r\n");
		return XST_FAILURE;
	}
	BufferAddr = (UINTPTR)Packet;
	BdCurPtr = BdPtr;
	for(Index = 0; Index< NUM_BD_TO_TRANSFER; Index++){
		u32 CrBits = 0;
		Status = XAxiDma_BdSetBufAddr(BdCurPtr, (UINTPTR) BufferAddr);
		if (Status != XST_SUCCESS) {
			xil_printf("Tx set buffer addr %x on BD %x failed %d\r\n",
				(UINTPTR)BufferAddr, (UINTPTR)BdCurPtr, Status);
			return XST_FAILURE;
		}
		Status = XAxiDma_BdSetLength(BdCurPtr, MAX_PKT_LEN,
						TxRingPtr->MaxTransferLen);
		if (Status != XST_SUCCESS) {
			xil_printf("Tx set length %d on BD %x failed %d\r\n",
				MAX_PKT_LEN, (UINTPTR)BdCurPtr, Status);
			return XST_FAILURE;
		}
		if (Index == 0) {
			/* The first BD has SOF set */
			CrBits |= XAXIDMA_BD_CTRL_TXSOF_MASK;
		}

		if(Index == (NUM_BD_TO_TRANSFER - 1)) {
			/* The last BD should have EOF and IOC set */
			CrBits |= XAXIDMA_BD_CTRL_TXEOF_MASK;
			/*Point the next pointer back to the first BD so we cycle*/
			XAxiDma_BdWrite((BdCurPtr), XAXIDMA_BD_NDESC_OFFSET, (int)BdPtr);
		}

		XAxiDma_BdSetCtrl(BdCurPtr, CrBits);
		XAxiDma_BdSetId(BdCurPtr, (UINTPTR)BufferAddr);
		BufferAddr += MAX_PKT_LEN;
		BdCurPtr = (XAxiDma_Bd *)XAxiDma_BdRingNext(TxRingPtr, BdCurPtr);
	}
	/* Give the BD to DMA to kick off the transmission. */
	Status = XAxiDma_BdRingToHw(TxRingPtr, NUM_BD_TO_TRANSFER, BdPtr);
	if (Status != XST_SUCCESS) {
		xil_printf("to hw failed %d\r\n", Status);
		return XST_FAILURE;
	}
	Status = XAxiDma_SelectCyclicMode(AxiDmaInstPtr, XAXIDMA_DMA_TO_DEVICE, TRUE);
	if (Status != XST_SUCCESS){
		xil_printf("Failed to set Cyclic mode on TX\r\n");
		return XST_FAILURE;
	}

	Status = XAxiDma_BdRingStart(TxRingPtr);
	if (Status != XST_SUCCESS){
		xil_printf("TX failed start bdring txsetup %d\r\n", Status);
		return XST_FAILURE;
	}

	Xil_Out32((XPAR_AXIDMA_0_BASEADDR + XAXIDMA_TX_OFFSET + XAXIDMA_TDESC_OFFSET), 0x140);
	// XXX BUG here?
	return XST_SUCCESS;
}

