/*
 * Copyright (c) 2016, Freescale Semiconductor, Inc.
 * Copyright 2016-2018 NXP
 * All rights reserved.
 *
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "fsl_flexspi.h"
#include "app.h"
#include "fsl_debug_console.h"
#include "fsl_cache.h"

#include "pin_mux.h"
#include "clock_config.h"
#include "board.h"
#include "MIMXRT1062.h"
#include "fsl_common.h"
#include "usb_host_config.h"
#include "usb_host.h"
#include "fsl_device_registers.h"
#include "usb_host_msd.h"
#include "host_msd_fatfs.h"
#include "fsl_common.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "board.h"
#if (defined(FSL_FEATURE_SOC_SYSMPU_COUNT) && (FSL_FEATURE_SOC_SYSMPU_COUNT > 0U))
#include "fsl_sysmpu.h"
#endif /* FSL_FEATURE_SOC_SYSMPU_COUNT */
#include "app.h"

#if ((!USB_HOST_CONFIG_KHCI) && (!USB_HOST_CONFIG_EHCI) && (!USB_HOST_CONFIG_OHCI) && (!USB_HOST_CONFIG_IP3516HS))
#error Please enable USB_HOST_CONFIG_KHCI, USB_HOST_CONFIG_EHCI, USB_HOST_CONFIG_OHCI, or USB_HOST_CONFIG_IP3516HS in file usb_host_config.
#endif

#include "usb_phy.h"
/*******************************************************************************
 * Definitions
 ******************************************************************************/
/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Variables
 ******************************************************************************/
/* Program data buffer should be 4-bytes alignment, which can avoid busfault due to this memory region is configured as
   Device Memory by MPU. */
SDK_ALIGN(static uint8_t s_nor_program_buffer[256], 4);
static uint8_t s_nor_read_buffer[256];

extern status_t flexspi_nor_flash_erase_sector(FLEXSPI_Type *base, uint32_t address);
extern status_t flexspi_nor_flash_page_program(FLEXSPI_Type *base, uint32_t dstAddr, const uint32_t *src);
extern status_t flexspi_nor_get_vendor_id(FLEXSPI_Type *base, uint8_t *vendorId);
extern status_t flexspi_nor_enable_quad_mode(FLEXSPI_Type *base);
extern status_t flexspi_nor_erase_chip(FLEXSPI_Type *base);
extern void flexspi_nor_flash_init(FLEXSPI_Type *base);
/*******************************************************************************
 * Code
 ******************************************************************************/
flexspi_device_config_t deviceconfig = {
		.flexspiRootClk       = 133000000,
		.flashSize            = FLASH_SIZE,
		.CSIntervalUnit       = kFLEXSPI_CsIntervalUnit1SckCycle,
		.CSInterval           = 2,
		.CSHoldTime           = 3,
		.CSSetupTime          = 3,
		.dataValidTime        = 0,
		.columnspace          = 0,
		.enableWordAddress    = 0,
		.AWRSeqIndex          = 0,
		.AWRSeqNumber         = 0,
		.ARDSeqIndex          = NOR_CMD_LUT_SEQ_IDX_READ_FAST_QUAD,
		.ARDSeqNumber         = 1,
		.AHBWriteWaitUnit     = kFLEXSPI_AhbWriteWaitUnit2AhbCycle,
		.AHBWriteWaitInterval = 0,
};

const uint32_t customLUT[CUSTOM_LUT_LENGTH] = {
		/* Normal read mode -SDR */
		[4 * NOR_CMD_LUT_SEQ_IDX_READ_NORMAL] =
				FLEXSPI_LUT_SEQ(kFLEXSPI_Command_SDR, kFLEXSPI_1PAD, 0x03, kFLEXSPI_Command_RADDR_SDR, kFLEXSPI_1PAD, 0x18),
				[4 * NOR_CMD_LUT_SEQ_IDX_READ_NORMAL + 1] =
						FLEXSPI_LUT_SEQ(kFLEXSPI_Command_READ_SDR, kFLEXSPI_1PAD, 0x04, kFLEXSPI_Command_STOP, kFLEXSPI_1PAD, 0),

						/* Fast read mode - SDR */
						[4 * NOR_CMD_LUT_SEQ_IDX_READ_FAST] =
								FLEXSPI_LUT_SEQ(kFLEXSPI_Command_SDR, kFLEXSPI_1PAD, 0x0B, kFLEXSPI_Command_RADDR_SDR, kFLEXSPI_1PAD, 0x18),
								[4 * NOR_CMD_LUT_SEQ_IDX_READ_FAST + 1] = FLEXSPI_LUT_SEQ(
										kFLEXSPI_Command_DUMMY_SDR, kFLEXSPI_1PAD, 0x08, kFLEXSPI_Command_READ_SDR, kFLEXSPI_1PAD, 0x04),

										/* Fast read quad mode - SDR */
										[4 * NOR_CMD_LUT_SEQ_IDX_READ_FAST_QUAD] =
												FLEXSPI_LUT_SEQ(kFLEXSPI_Command_SDR, kFLEXSPI_1PAD, 0xEB, kFLEXSPI_Command_RADDR_SDR, kFLEXSPI_4PAD, 0x18),
												[4 * NOR_CMD_LUT_SEQ_IDX_READ_FAST_QUAD + 1] = FLEXSPI_LUT_SEQ(
														kFLEXSPI_Command_DUMMY_SDR, kFLEXSPI_4PAD, 0x06, kFLEXSPI_Command_READ_SDR, kFLEXSPI_4PAD, 0x04),

														/* Read extend parameters */
														[4 * NOR_CMD_LUT_SEQ_IDX_READSTATUS] =
																FLEXSPI_LUT_SEQ(kFLEXSPI_Command_SDR, kFLEXSPI_1PAD, 0x81, kFLEXSPI_Command_READ_SDR, kFLEXSPI_1PAD, 0x04),

																/* Write Enable */
																[4 * NOR_CMD_LUT_SEQ_IDX_WRITEENABLE] =
																		FLEXSPI_LUT_SEQ(kFLEXSPI_Command_SDR, kFLEXSPI_1PAD, 0x06, kFLEXSPI_Command_STOP, kFLEXSPI_1PAD, 0),

																		/* Erase Sector  */
																		[4 * NOR_CMD_LUT_SEQ_IDX_ERASESECTOR] =
																				FLEXSPI_LUT_SEQ(kFLEXSPI_Command_SDR, kFLEXSPI_1PAD, 0xD7, kFLEXSPI_Command_RADDR_SDR, kFLEXSPI_1PAD, 0x18),

																				/* Page Program - single mode */
																				[4 * NOR_CMD_LUT_SEQ_IDX_PAGEPROGRAM_SINGLE] =
																						FLEXSPI_LUT_SEQ(kFLEXSPI_Command_SDR, kFLEXSPI_1PAD, 0x02, kFLEXSPI_Command_RADDR_SDR, kFLEXSPI_1PAD, 0x18),
																						[4 * NOR_CMD_LUT_SEQ_IDX_PAGEPROGRAM_SINGLE + 1] =
																								FLEXSPI_LUT_SEQ(kFLEXSPI_Command_WRITE_SDR, kFLEXSPI_1PAD, 0x04, kFLEXSPI_Command_STOP, kFLEXSPI_1PAD, 0),

																								/* Page Program - quad mode */
																								[4 * NOR_CMD_LUT_SEQ_IDX_PAGEPROGRAM_QUAD] =
																										FLEXSPI_LUT_SEQ(kFLEXSPI_Command_SDR, kFLEXSPI_1PAD, 0x32, kFLEXSPI_Command_RADDR_SDR, kFLEXSPI_1PAD, 0x18),
																										[4 * NOR_CMD_LUT_SEQ_IDX_PAGEPROGRAM_QUAD + 1] =
																												FLEXSPI_LUT_SEQ(kFLEXSPI_Command_WRITE_SDR, kFLEXSPI_4PAD, 0x04, kFLEXSPI_Command_STOP, kFLEXSPI_1PAD, 0),

																												/* Read ID */
																												[4 * NOR_CMD_LUT_SEQ_IDX_READID] =
																														FLEXSPI_LUT_SEQ(kFLEXSPI_Command_SDR, kFLEXSPI_1PAD, 0x9F, kFLEXSPI_Command_READ_SDR, kFLEXSPI_1PAD, 0x04),

																														/* Enable Quad mode */
																														[4 * NOR_CMD_LUT_SEQ_IDX_WRITESTATUSREG] =
																																FLEXSPI_LUT_SEQ(kFLEXSPI_Command_SDR, kFLEXSPI_1PAD, 0x01, kFLEXSPI_Command_WRITE_SDR, kFLEXSPI_1PAD, 0x04),

																																/* Enter QPI mode */
																																[4 * NOR_CMD_LUT_SEQ_IDX_ENTERQPI] =
																																		FLEXSPI_LUT_SEQ(kFLEXSPI_Command_SDR, kFLEXSPI_1PAD, 0x35, kFLEXSPI_Command_STOP, kFLEXSPI_1PAD, 0),

																																		/* Exit QPI mode */
																																		[4 * NOR_CMD_LUT_SEQ_IDX_EXITQPI] =
																																				FLEXSPI_LUT_SEQ(kFLEXSPI_Command_SDR, kFLEXSPI_4PAD, 0xF5, kFLEXSPI_Command_STOP, kFLEXSPI_1PAD, 0),

																																				/* Read status register */
																																				[4 * NOR_CMD_LUT_SEQ_IDX_READSTATUSREG] =
																																						FLEXSPI_LUT_SEQ(kFLEXSPI_Command_SDR, kFLEXSPI_1PAD, 0x05, kFLEXSPI_Command_READ_SDR, kFLEXSPI_1PAD, 0x04),

																																						/* Erase whole chip */
																																						[4 * NOR_CMD_LUT_SEQ_IDX_ERASECHIP] =
																																								FLEXSPI_LUT_SEQ(kFLEXSPI_Command_SDR, kFLEXSPI_1PAD, 0xC7, kFLEXSPI_Command_STOP, kFLEXSPI_1PAD, 0),
};
static usb_status_t USB_HostEvent(usb_device_handle deviceHandle,
		usb_host_configuration_handle configurationHandle,
		uint32_t eventCode);

/*!
 * @brief app initialization.
 */
 static void USB_HostApplicationInit(void);

 extern void USB_HostClockInit(void);
 extern void USB_HostIsrEnable(void);
 extern void USB_HostTaskFn(void *param);
 void BOARD_InitHardware(void);

 /*******************************************************************************
  * Variables
  ******************************************************************************/

 /*! @brief USB host msd fatfs instance global variable */
 extern usb_host_msd_fatfs_instance_t g_MsdFatfsInstance;
 usb_host_handle g_HostHandle;

 /*******************************************************************************
  * Code
  ******************************************************************************/

 void USB_OTG1_IRQHandler(void)
 {
	 USB_HostEhciIsrFunction(g_HostHandle);
 }

 void USB_OTG2_IRQHandler(void)
 {
	 USB_HostEhciIsrFunction(g_HostHandle);
 }

 void USB_HostClockInit(void)
 {
	 usb_phy_config_struct_t phyConfig = {
			 BOARD_USB_PHY_D_CAL,
			 BOARD_USB_PHY_TXCAL45DP,
			 BOARD_USB_PHY_TXCAL45DM,
	 };

	 if (CONTROLLER_ID == kUSB_ControllerEhci0)
	 {
		 CLOCK_EnableUsbhs0PhyPllClock(kCLOCK_Usbphy480M, 480000000U);
		 CLOCK_EnableUsbhs0Clock(kCLOCK_Usb480M, 480000000U);
	 }
	 else
	 {
		 CLOCK_EnableUsbhs1PhyPllClock(kCLOCK_Usbphy480M, 480000000U);
		 CLOCK_EnableUsbhs1Clock(kCLOCK_Usb480M, 480000000U);
	 }
	 USB_EhciPhyInit(CONTROLLER_ID, BOARD_XTAL0_CLK_HZ, &phyConfig);
 }

 void USB_HostIsrEnable(void)
 {
	 uint8_t irqNumber;

	 uint8_t usbHOSTEhciIrq[] = USBHS_IRQS;
	 irqNumber                = usbHOSTEhciIrq[CONTROLLER_ID - kUSB_ControllerEhci0];
	 /* USB_HOST_CONFIG_EHCI */

	 /* Install isr, set priority, and enable IRQ. */
#if defined(__GIC_PRIO_BITS)
	 GIC_SetPriority((IRQn_Type)irqNumber, USB_HOST_INTERRUPT_PRIORITY);
#else
	 NVIC_SetPriority((IRQn_Type)irqNumber, USB_HOST_INTERRUPT_PRIORITY);
#endif
	 EnableIRQ((IRQn_Type)irqNumber);
 }

 void USB_HostTaskFn(void *param)
 {
	 USB_HostEhciTaskFunction(param);
 }

 /*!
  * @brief USB isr function.
  */

#if ((defined USB_HOST_CONFIG_COMPLIANCE_TEST) && (USB_HOST_CONFIG_COMPLIANCE_TEST))
 extern usb_status_t USB_HostTestEvent(usb_device_handle deviceHandle,
		 usb_host_configuration_handle configurationHandle,
		 uint32_t eventCode);
#endif

static usb_status_t USB_HostEvent(usb_device_handle deviceHandle,
		usb_host_configuration_handle configurationHandle,
		uint32_t eventCode)
{
#if ((defined USB_HOST_CONFIG_COMPLIANCE_TEST) && (USB_HOST_CONFIG_COMPLIANCE_TEST))
	usb_host_configuration_t *configuration;
	usb_status_t status1;
	usb_status_t status2;
	uint8_t interfaceIndex = 0;
#endif
	usb_status_t status = kStatus_USB_Success;
	switch (eventCode & 0x0000FFFFU)
	{
	case kUSB_HostEventAttach:
#if ((defined USB_HOST_CONFIG_COMPLIANCE_TEST) && (USB_HOST_CONFIG_COMPLIANCE_TEST))
		status1 = USB_HostTestEvent(deviceHandle, configurationHandle, eventCode);
		status2 = USB_HostMsdEvent(deviceHandle, configurationHandle, eventCode);
		if ((status1 == kStatus_USB_NotSupported) && (status2 == kStatus_USB_NotSupported))
		{
			status = kStatus_USB_NotSupported;
		}
#else
	status = USB_HostMsdEvent(deviceHandle, configurationHandle, eventCode);
#endif
	break;

	case kUSB_HostEventNotSupported:
#if ((defined USB_HOST_CONFIG_COMPLIANCE_TEST) && (USB_HOST_CONFIG_COMPLIANCE_TEST))
		configuration = (usb_host_configuration_t *)configurationHandle;
		for (interfaceIndex = 0; interfaceIndex < configuration->interfaceCount; ++interfaceIndex)
		{
			if (((usb_descriptor_interface_t *)configuration->interfaceList[interfaceIndex].interfaceDesc)
					->bInterfaceClass == 9U) /* 9U is hub class code */
			{
				break;
			}
		}

		if (interfaceIndex < configuration->interfaceCount)
		{
			usb_echo("unsupported hub\r\n");
		}
		else
		{
			usb_echo("Unsupported Device\r\n");
		}
#else
		usb_echo("Unsupported Device\r\n");
#endif
		break;

	case kUSB_HostEventEnumerationDone:
#if ((defined USB_HOST_CONFIG_COMPLIANCE_TEST) && (USB_HOST_CONFIG_COMPLIANCE_TEST))
		status1 = USB_HostTestEvent(deviceHandle, configurationHandle, eventCode);
		status2 = USB_HostMsdEvent(deviceHandle, configurationHandle, eventCode);
		if ((status1 != kStatus_USB_Success) && (status2 != kStatus_USB_Success))
		{
			status = kStatus_USB_Error;
		}
#else
	status = USB_HostMsdEvent(deviceHandle, configurationHandle, eventCode);
#endif
	break;

	case kUSB_HostEventDetach:
#if ((defined USB_HOST_CONFIG_COMPLIANCE_TEST) && (USB_HOST_CONFIG_COMPLIANCE_TEST))
		status1 = USB_HostTestEvent(deviceHandle, configurationHandle, eventCode);
		status2 = USB_HostMsdEvent(deviceHandle, configurationHandle, eventCode);
		if ((status1 != kStatus_USB_Success) && (status2 != kStatus_USB_Success))
		{
			status = kStatus_USB_Error;
		}
#else
	status = USB_HostMsdEvent(deviceHandle, configurationHandle, eventCode);
#endif
	break;

	case kUSB_HostEventEnumerationFail:
		usb_echo("enumeration failed\r\n");
		break;

	default:
		break;
	}
	return status;
}

static void USB_HostApplicationInit(void)
{
	usb_status_t status = kStatus_USB_Success;

	USB_HostClockInit();

#if ((defined FSL_FEATURE_SOC_SYSMPU_COUNT) && (FSL_FEATURE_SOC_SYSMPU_COUNT))
	SYSMPU_Enable(SYSMPU, 0);
#endif /* FSL_FEATURE_SOC_SYSMPU_COUNT */

	status = USB_HostInit(CONTROLLER_ID, &g_HostHandle, USB_HostEvent);
	if (status != kStatus_USB_Success)
	{
		usb_echo("host init error\r\n");
		return;
	}
	USB_HostIsrEnable();

	usb_echo("host init done\r\n");
}

int main(void)
{
	uint32_t i = 0;
	status_t status;
	uint8_t vendorID = 0;

	BOARD_ConfigMPU();
	BOARD_InitBootPins();
	BOARD_InitBootClocks();
	BOARD_InitDebugConsole();

	flexspi_nor_flash_init(EXAMPLE_FLEXSPI);

	PRINTF("\r\nFLEXSPI example started!\r\n");
	/* Enter quad mode. */
	status = flexspi_nor_enable_quad_mode(EXAMPLE_FLEXSPI);
	if (status != kStatus_Success)
	{
		return status;
	}

	/* Erase sectors. */
	PRINTF("Erasing Serial NOR over FlexSPI...\r\n");
	status = flexspi_nor_flash_erase_sector(EXAMPLE_FLEXSPI, EXAMPLE_SECTOR * SECTOR_SIZE);
	if (status != kStatus_Success)
	{
		PRINTF("Erase sector failure !\r\n");
		return -1;
	}

	for (i = 0; i < 0xFFU; i++)
	{
		s_nor_program_buffer[i] = i;
	}
	uint32_t data = 0xA0807E7;
	uint32_t tempo = 0xE3A1E;
	uint32_t ID = 0;
	uint32_t ID_TAG = 0;
	uint32_t BIO_ID = 0;
	uint32_t permission = 0;
	uint32_t accessMethod = 0;

/*	SDK_ALIGN(static uint32_t memoryData[ 40 ],4);
	memoryData[ 0 ] = data;
	memoryData[ 1 ] = permission << 28 | accessMethod << 24 | tempo;
	memoryData[ 2 ] = ID;
	memoryData[ 3 ] = ID_TAG;
	memoryData[ 4 ] = BIO_ID;

	data = 0xA0807E8;
	tempo = 0;
	ID = 0;
	ID_TAG = 0;
	BIO_ID = 0;
	permission = 5;
	accessMethod = 5;

	memoryData[ 5 ] = data;
	memoryData[ 6 ] = permission << 28 | accessMethod << 24 | tempo;
	memoryData[ 7 ] = ID;
	memoryData[ 8 ] = ID_TAG;
	memoryData[ 9 ] = BIO_ID;

	data = 0xA0807E8;
	tempo = 0xE3A1f;
	ID = 20;
	ID_TAG = 31;
	BIO_ID = 0;
	permission = 2;
	accessMethod = 2;

	memoryData[ 10 ] = data;
	memoryData[ 11 ] = permission << 28 | accessMethod << 24 | tempo;
	memoryData[ 12 ] = ID;
	memoryData[ 13 ] = ID_TAG;
	memoryData[ 14 ] = BIO_ID;

	data = 0xA0807E8;
	tempo = 0xE3A1f;
	ID = 0;
	ID_TAG = 41;
	BIO_ID = 1;
	permission = 0;
	accessMethod = 3;

	memoryData[ 15 ] = data;
	memoryData[ 16 ] = permission << 28 | accessMethod << 24 | tempo;
	memoryData[ 17 ] = ID;
	memoryData[ 18 ] = ID_TAG;
	memoryData[ 19 ] = BIO_ID;

	data = 0xA0807E8;
	tempo = 0xE3A1f;
	ID = 0;
	ID_TAG = 10;
	BIO_ID = 0;
	permission = 1;
	accessMethod = 4;

	memoryData[ 20 ] = data;
	memoryData[ 21 ] = permission << 28 | accessMethod << 24 | tempo;
	memoryData[ 22 ] = ID;
	memoryData[ 23 ] = ID_TAG;
	memoryData[ 24 ] = BIO_ID;

	data = 0xA0807E8;
	tempo = 0xE3A1f;
	ID = 51;
	ID_TAG = 40;
	BIO_ID = 1;
	permission = 4;
	accessMethod = 1;

	memoryData[ 25 ] = data;
	memoryData[ 26 ] = permission << 28 | accessMethod << 24 | tempo;
	memoryData[ 27 ] = ID;
	memoryData[ 28 ] = ID_TAG;
	memoryData[ 29 ] = BIO_ID;

	data = 0xA0807E8;
	tempo = 0xE3A1f;
	ID = 61;
	ID_TAG = 10101;
	BIO_ID = 0;
	permission = 1;
	accessMethod = 1;

	memoryData[ 30 ] = data;
	memoryData[ 31 ] = permission << 28 | accessMethod << 24 | tempo;
	memoryData[ 32 ] = ID;
	memoryData[ 33 ] = ID_TAG;
	memoryData[ 34 ] = BIO_ID;

	data = 0xA0807E8;
	tempo = 0xE3A1f;
	ID = 71;
	ID_TAG = 45621;
	BIO_ID = 1;
	permission = 5;
	accessMethod = 6;

	memoryData[ 35 ] = data;
	memoryData[ 36 ] = permission << 28 | accessMethod << 24 | tempo;
	memoryData[ 37 ] = ID;
	memoryData[ 38 ] = ID_TAG;
	memoryData[ 39 ] = BIO_ID;
	status =//flexspi_nor_flash_program(EXAMPLE_FLEXSPI, EXAMPLE_SECTOR * SECTOR_SIZE, (void *)memoryData, 160);
			flexspi_nor_flash_page_program(EXAMPLE_FLEXSPI, EXAMPLE_SECTOR * SECTOR_SIZE, (void *)memoryData);
*/	if (status != kStatus_Success)
	{
		PRINTF("Page program failure !\r\n");
		return -1;
	}

	USB_HostApplicationInit();

	setArquivoGravado(false);
	while (1)
	{
		USB_HostTaskFn(g_HostHandle);
		USB_HostMsdTask(&g_MsdFatfsInstance);
        if(getArquivoGravado())
        {
#if 1

        	USB2->USBCMD |= (1 << USB_USBCMD_RST_MASK);
            CLOCK_DisableUsbhs1PhyPllClock();

        	static uint32_t s_resetEntry = 0;
			static uint32_t s_stackPointer = 0;
			s_resetEntry = *(uint32_t *)(0x60400000 + 4);
			s_stackPointer = *(uint32_t *)0x60400000;
			// Turn off interrupts.
			__disable_irq();

			NVIC->ISER[3] = 0;
			// Set the VTOR.
			SCB->VTOR = 0x60400000;

			// Memory barriers for good measure.
			__ISB();
			__DSB();

			// Set main stack pointer and process stack pointer.
			__set_MSP(s_stackPointer);
			__set_PSP(s_stackPointer);

			// Jump to application entry point, does not return.
			static void (*s_entry)(void) = 0;
			s_entry = (void (*)(void))s_resetEntry;
			s_entry();
#endif

#if 0
			printf("\r\nMFB: Jump to Application code at 0x%x.\r\n", EXAMPLE_FLEXSPI_AMBA_BASE + 60000);
			printf("-------------------------------------\r\n");
			static uint32_t s_resetEntry = 0;
			static uint32_t s_stackPointer = 0;
			s_resetEntry = *(uint32_t *)(0x60060000 + 4);
			s_stackPointer = *(uint32_t *)0x60060000;
			// Turn off interrupts.
			__disable_irq();

			// Set the VTOR.
			SCB->VTOR = 0x60060000;

			// Memory barriers for good measure.
			__ISB();
			__DSB();

			// Set main stack pointer and process stack pointer.
			__set_MSP(s_stackPointer);
			__set_PSP(s_stackPointer);

			// Jump to application entry point, does not return.
			static void (*s_entry)(void) = 0;
			s_entry = (void (*)(void))s_resetEntry;
			s_entry();
#endif


		    // Desative as interrupções antes de saltar
		    __disable_irq();

		    // Limpe o cache de instruções (se aplicável)
		    SCB_DisableDCache(); // Desabilitar cache de dados (se aplicável)
		    SCB_DisableICache(); // Desabilitar cache de instruções (se aplicável)

		    // Crie um ponteiro de função para o endereço desejado
		    void (*jump_to_app)(void) = (void (*)(void))0x60060000;

		    // Faça o salto para o endereço
		    jump_to_app();





        	/*
        	USB->USBCMD |= (1 << USB_USBCMD_RST_MASK);
            __disable_irq();
            CLOCK_DisableUsbhs0PhyPllClock();
            //USB_EhciPhyDeinit(CONTROLLER_ID);
            //USB_HostMsdTask(&g_MsdFatfsInstance);
            uint32_t destAdrss = 0x60060000;//0x60042000;
            static uint32_t s_resetEntry = 0;
            static uint32_t s_stackPointer = 0;
            SCB->VTOR = destAdrss;
            __ISB();
            __DSB();
            s_resetEntry = *(uint32_t *)(destAdrss + 4);
            s_stackPointer = *(uint32_t *)destAdrss;
            __set_MSP(s_stackPointer);
            __set_PSP(s_stackPointer);
            static void (*s_entry)(void) = 0;
            s_entry = (void (*)(void))s_resetEntry;
            s_entry();
            */
        }
	}

	/* Get vendor ID. */
	status = flexspi_nor_get_vendor_id(EXAMPLE_FLEXSPI, &vendorID);
	if (status != kStatus_Success)
	{
		return status;
	}
	PRINTF("Vendor ID: 0x%x\r\n", vendorID);


#if !(defined(XIP_EXTERNAL_FLASH))
    		/* Erase whole chip . */
			PRINTF("Erasing whole chip over FlexSPI...\r\n");

status = flexspi_nor_erase_chip(EXAMPLE_FLEXSPI);
if (status != kStatus_Success)
{
	return status;
}
PRINTF("Erase finished !\r\n");

#endif

/* Enter quad mode. */
status = flexspi_nor_enable_quad_mode(EXAMPLE_FLEXSPI);
if (status != kStatus_Success)
{
	return status;
}

/* Erase sectors. */
PRINTF("Erasing Serial NOR over FlexSPI...\r\n");
status = flexspi_nor_flash_erase_sector(EXAMPLE_FLEXSPI, EXAMPLE_SECTOR * SECTOR_SIZE);
if (status != kStatus_Success)
{
	PRINTF("Erase sector failure !\r\n");
	return -1;
}

memset(s_nor_program_buffer, 0xFFU, sizeof(s_nor_program_buffer));

DCACHE_InvalidateByRange(EXAMPLE_FLEXSPI_AMBA_BASE + EXAMPLE_SECTOR * SECTOR_SIZE, FLASH_PAGE_SIZE);

memcpy(s_nor_read_buffer, (void *)(EXAMPLE_FLEXSPI_AMBA_BASE + EXAMPLE_SECTOR * SECTOR_SIZE),
		sizeof(s_nor_read_buffer));

if (memcmp(s_nor_program_buffer, s_nor_read_buffer, sizeof(s_nor_program_buffer)))
{
	PRINTF("Erase data -  read out data value incorrect !\r\n ");
	return -1;
}
else
{
	PRINTF("Erase data - successfully. \r\n");
}

for (i = 0; i < 0xFFU; i++)
{
	s_nor_program_buffer[i] = i;
}

status =
		flexspi_nor_flash_page_program(EXAMPLE_FLEXSPI, EXAMPLE_SECTOR * SECTOR_SIZE, (void *)s_nor_program_buffer);
if (status != kStatus_Success)
{
	PRINTF("Page program failure !\r\n");
	return -1;
}

DCACHE_InvalidateByRange(EXAMPLE_FLEXSPI_AMBA_BASE + EXAMPLE_SECTOR * SECTOR_SIZE, FLASH_PAGE_SIZE);

memcpy(s_nor_read_buffer, (void *)(EXAMPLE_FLEXSPI_AMBA_BASE + EXAMPLE_SECTOR * SECTOR_SIZE),
		sizeof(s_nor_read_buffer));

if (memcmp(s_nor_read_buffer, s_nor_program_buffer, sizeof(s_nor_program_buffer)) != 0)
{
	PRINTF("Program data -  read out data value incorrect !\r\n ");
	return -1;
}
else
{
	PRINTF("Program data - successfully. \r\n");
}

while (1)
{
}
}
