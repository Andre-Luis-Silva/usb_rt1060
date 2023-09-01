/*
 * Copyright (c) 2015 - 2016, Freescale Semiconductor, Inc.
 * Copyright 2016, 2018 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include "usb_host_config.h"
#include "usb_host.h"
#include "usb_host_msd.h"
#include "host_msd_fatfs.h"
#include "ff.h"
#include "diskio.h"
#include "fsl_device_registers.h"
#include "app.h"
#include "fsl_cache.h"



/*******************************************************************************
 * Definitions
 ******************************************************************************/

#if MSD_FATFS_THROUGHPUT_TEST_ENABLE
#include "fsl_device_registers.h"
#define THROUGHPUT_BUFFER_SIZE (64 * 1024) /* throughput test buffer */
#define MCU_CORE_CLOCK         (120000000) /* mcu core clock, user need to configure it. */
#endif                                     /* MSD_FATFS_THROUGHPUT_TEST_ENABLE */

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*!
 * @brief host msd control transfer callback.
 *
 * This function is used as callback function for control transfer .
 *
 * @param param      the host msd fatfs instance pointer.
 * @param data       data buffer pointer.
 * @param dataLength data length.
 * @status           transfer result status.
 */
void USB_HostMsdControlCallback(void *param, uint8_t *data, uint32_t dataLength, usb_status_t status);

/*!
 * @brief msd fatfs test code execute done.
 */
static void USB_HostMsdFatfsTestDone(void);

#if ((defined MSD_FATFS_THROUGHPUT_TEST_ENABLE) && (MSD_FATFS_THROUGHPUT_TEST_ENABLE))
/*!
 * @brief host msd fatfs throughput test.
 *
 * @param msdFatfsInstance   the host fatfs instance pointer.
 */
static void USB_HostMsdFatfsThroughputTest(usb_host_msd_fatfs_instance_t *msdFatfsInstance);

#else

/*!
 * @brief display file information.
 */
static void USB_HostMsdFatfsDisplayFileInfo(FILINFO *fileInfo);

/*!
 * @brief list files and sub-directory in one directory, the function don't check all sub-directories recursively.
 */
static FRESULT USB_HostMsdFatfsListDirectory(const TCHAR *path);

/*!
 * @brief forward function pointer for fatfs f_forward function.
 *
 * @param data_ptr   forward data pointer.
 * @param dataLength data length.
 */
#if _USE_FORWARD && _FS_TINY
static uint32_t USB_HostMsdFatfsForward(const uint8_t *data_ptr, uint32_t dataLength);
#endif

/*!
 * @brief host msd fatfs test.
 *
 * This function implements msd fatfs test.
 *
 * @param msdFatfsInstance   the host fatfs instance pointer.
 */
static void USB_HostMsdFatfsTest(usb_host_msd_fatfs_instance_t *msdFatfsInstance);

#endif /* MSD_FATFS_THROUGHPUT_TEST_ENABLE */

#if ((defined USB_HOST_CONFIG_COMPLIANCE_TEST) && (USB_HOST_CONFIG_COMPLIANCE_TEST))
extern usb_status_t USB_HostTestModeInit(usb_device_handle deviceHandle);
#endif

extern status_t flexspi_nor_flash_erase_sector(FLEXSPI_Type *base, uint32_t address);
extern status_t flexspi_nor_flash_page_program(FLEXSPI_Type *base, uint32_t dstAddr, const uint32_t *src);
extern status_t flexspi_nor_enable_quad_mode(FLEXSPI_Type *base);
extern status_t flexspi_nor_get_vendor_id(FLEXSPI_Type *base, uint8_t *vendorId);

static bool isFirstAddr = true;
static protocol_hex_firmware_t firmwareLine;
static bool initialAddress = true;
uint32_t addrToFirmware;
bool _packageFirmwareReady = false;
static bool isLastPackage = false;
protocol_firmware_buff_recp_t ctrlFirmwareBuffer = { 0, 0, 0 };
uint8_t firmwareData[512];
static uint16_t bytesCount = 0;
static uint32_t addrToSaveFirmware;

static bool flag = false;
/*******************************************************************************
 * Variables
 ******************************************************************************/

/*! @brief msd class handle array for fatfs */
extern usb_host_class_handle g_UsbFatfsClassHandle;

usb_host_msd_fatfs_instance_t g_MsdFatfsInstance; /* global msd fatfs instance */
static FATFS fatfs;
/* control transfer on-going state. It should set to 1 when start control transfer, it is set to 0 in the callback */
volatile uint8_t controlIng;
/* control transfer callback status */
volatile usb_status_t controlStatus;

#if MSD_FATFS_THROUGHPUT_TEST_ENABLE
USB_DMA_NONINIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
static uint32_t testThroughputBuffer[THROUGHPUT_BUFFER_SIZE / 4]; /* the buffer for throughput test */
uint32_t testSizeArray[] = {20 * 1024, 20 * 1024};                /* test time and test size (uint: K)*/
#else
USB_DMA_NONINIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
static uint8_t testBuffer[(FF_MAX_SS > 256) ? FF_MAX_SS : 256]; /* normal test buffer */
#endif /* MSD_FATFS_THROUGHPUT_TEST_ENABLE */

/*******************************************************************************
 * Code
 ******************************************************************************/
#define ADDRESS	0x60014000
#define NO_LEADING_ZERO	0
#define WITH_LEADING_ZERO 1
#define NULL_CHARACTER 1
#define ERROR	0
#define OK		1
#define PERMITIDO	0
#define NEGADO		1
#define SENHA 		0
#define RFID_TAG	1
#define BIOMETRY	2
#define BYTES_QUANTITY		20
#define MASK_4BITS			0x0F
#define MASK_8BITS			0xFF
#define MASK_16BITS			0xFFFF
#define MASK_24BITS			0xFFFFFF
#define MOVEBITS_DAY		24
#define MOVEBITS_MONTH		16
#define MOVEBITS_HOUR		16
#define MOVEBITS_SECOND		8
#define MOVEBITS_PERMISSION	28
#define MOVEBITS_ACCES		24
#define POSITION_MEMORY0	0
#define POSITION_MEMORY1	4
#define POSITION_MEMORY2	8
#define POSITION_MEMORY3	12
#define POSITION_MEMORY4	16
#define MAX_DIGITS	10


uint32_t UIntToString( uint32_t value, char* string, uint32_t leftZero )
{
	uint8_t buffer[ MAX_DIGITS ], countBytes = 0;
	uint32_t valueAux = value;
	for( ; value; value /= MAX_DIGITS )
	{
		buffer[ countBytes++ ] = ( char )( '0' + value % MAX_DIGITS );
		if( valueAux < MAX_DIGITS && leftZero )
		{
			buffer[ countBytes++ ] = '0';
		}
	}
	for( int i = countBytes; i > 0; i-- )
	{
		*string++ = buffer[ i - 1 ];
	}
	return countBytes;
}

uint32_t StringToBuffer( char* string, char* buffer, uint32_t sizeString )
{
	uint32_t contCharacter;
	for( contCharacter = 0; contCharacter < sizeString; contCharacter++ )
	{
		*( buffer++ ) = *( string++ );
	}
	return contCharacter;
}

uint32_t FileCSVWrite( uint32_t addressData, FIL file )
{
	FRESULT fatfsCode;
	FATFS *fs;
	uint32_t resultSize;

	// Header of CSV file
	char* textCSV = "DATA,HORA,ID,ID RF,ID BIOMETRIA,PERMISSAO,METODO DE ACESSO\r\n";
	fatfsCode      = f_write(&file, textCSV, sizeof( "DATA,HORA,ID,ID RF,ID BIOMETRIA,PERMISSAO,METODO DE ACESSO\r\n" ) - NULL_CHARACTER, (UINT *)&resultSize);
	if ((fatfsCode) || (resultSize != ( sizeof( "DATA,HORA,ID,ID RF,ID BIOMETRIA,PERMISSAO,METODO DE ACESSO\r\n" ) - NULL_CHARACTER ) ) )
	{
		usb_echo("error\r\n");
		f_close(&file);
		USB_HostMsdFatfsTestDone();
		return ERROR;
	}
	fatfsCode = f_sync(&file);
	if (fatfsCode)
	{
		usb_echo("error\r\n");
		f_close(&file);
		USB_HostMsdFatfsTestDone();
		return ERROR;
	}

	uint32_t date;
	uint32_t tempo;
	uint32_t ID;
	uint32_t ID_TAG;
	uint32_t BIO_ID ;
	uint32_t permission;
	uint32_t accessMethod;
	uint32_t writtenAddress;
	uint32_t countData = 0;

	while( ( *(uint32_t *)(addressData + countData) != 0x00000000 ) && ( *(uint32_t *)(addressData + countData) != 0xFFFFFFFF ) )
	{
		date = *(uint32_t *)(addressData + countData + POSITION_MEMORY0);
		tempo = ( *(uint32_t *)(addressData + countData + POSITION_MEMORY1) ) & MASK_24BITS;
		permission = ( ( *(uint32_t *)(addressData + countData + POSITION_MEMORY1) ) >> MOVEBITS_PERMISSION ) & MASK_4BITS;
		accessMethod = ( ( *(uint32_t *)(addressData + countData + POSITION_MEMORY1) ) >> MOVEBITS_ACCES ) & MASK_4BITS;;
		ID = ( *(uint32_t *)(addressData + countData + POSITION_MEMORY2) ) ;
		ID_TAG = ( *(uint32_t *)(addressData + countData + POSITION_MEMORY3) ) ;
		BIO_ID = ( *(uint32_t *)(addressData + countData + POSITION_MEMORY4) ) ;

		writtenAddress = 0;

		// Convert to string the date saved on the flash and save in a buffer
		if( date != 0 )
		{
			writtenAddress += UIntToString( date >> MOVEBITS_DAY, ( testBuffer + writtenAddress ), WITH_LEADING_ZERO );
			testBuffer[ writtenAddress++ ] = '/';
			writtenAddress += UIntToString( ( date >> MOVEBITS_MONTH ) & MASK_8BITS, ( testBuffer + writtenAddress ), WITH_LEADING_ZERO );
			testBuffer[ writtenAddress++ ] = '/';
			writtenAddress += UIntToString( ( date & MASK_16BITS ), ( testBuffer + writtenAddress ), NO_LEADING_ZERO );
		}
		else
		{
			testBuffer[ writtenAddress++ ] = '-';
		}
		testBuffer[ writtenAddress++ ] = ',';

		// Convert to string the time saved on the flash and save in a buffer
		if( ( tempo >> MOVEBITS_HOUR ) != 0 )
		{
			writtenAddress += UIntToString( tempo >> MOVEBITS_HOUR, ( testBuffer + writtenAddress ), WITH_LEADING_ZERO );
		}
		else
		{
			testBuffer[ writtenAddress++ ] = '0';
		}
		testBuffer[ writtenAddress++ ] = ':';

		if( ( tempo >> MOVEBITS_SECOND ) != 0 )
		{
			writtenAddress += UIntToString( ( tempo >> MOVEBITS_SECOND ) & MASK_8BITS, ( testBuffer + writtenAddress ), WITH_LEADING_ZERO );
		}
		else
		{
			testBuffer[ writtenAddress++ ] = '0';
		}
		testBuffer[ writtenAddress++ ] = ':';

		if( ( tempo & MASK_8BITS ) != 0 )
		{
			writtenAddress += UIntToString( tempo & MASK_8BITS, ( testBuffer + writtenAddress ), WITH_LEADING_ZERO );
		}
		else
		{
			testBuffer[ writtenAddress++ ] = '0';
		}
		testBuffer[ writtenAddress++ ] = ',';

		// Convert to string the ID saved on the flash and save in a buffer
		if( ID != 0 )
		{
			writtenAddress += UIntToString( ID, ( testBuffer + writtenAddress ), NO_LEADING_ZERO );
		}
		else
		{
			testBuffer[ writtenAddress++ ] = '-';
		}
		testBuffer[ writtenAddress++ ] = ',';

		// Convert to string the ID of TAG saved on the flash and save in a buffer
		if( ID_TAG != 0 )
		{
			writtenAddress += UIntToString( ID_TAG, ( testBuffer + writtenAddress ), NO_LEADING_ZERO );
		}
		else
		{
			testBuffer[ writtenAddress++ ] = '-';
		}
		testBuffer[ writtenAddress++ ] = ',';

		// Convert to string the ID of biometry saved on the flash and save in a buffer
		if( BIO_ID != 0 )
		{
			writtenAddress += UIntToString( BIO_ID, ( testBuffer + writtenAddress ), NO_LEADING_ZERO );
		}
		else
		{
			testBuffer[ writtenAddress++ ] = '-';
		}
		testBuffer[ writtenAddress++ ] = ',';

		// Verify the permission and convert the string to the type of permission
		if( permission == PERMITIDO )
		{
			writtenAddress += StringToBuffer( "PERMITIDO", ( testBuffer + writtenAddress ), sizeof( "PERMITIDO" ) - NULL_CHARACTER );
		}
		else if( permission == NEGADO )
		{
			writtenAddress += StringToBuffer( "NEGADO", ( testBuffer + writtenAddress ), sizeof( "NEGADO" ) - NULL_CHARACTER );
		}
		else
		{
			writtenAddress += StringToBuffer( "-", ( testBuffer + writtenAddress ), sizeof( "-" ) - NULL_CHARACTER );
		}
		testBuffer[ writtenAddress++ ] = ',';

		// Verify the access method and convert the string the to type of access method
		if( accessMethod == SENHA )
		{
			writtenAddress += StringToBuffer( "SENHA", ( testBuffer + writtenAddress ), sizeof( "SENHA" ) - NULL_CHARACTER );
		}
		else if( accessMethod == RFID_TAG )
		{
			writtenAddress += StringToBuffer( "RFID_TAG", ( testBuffer + writtenAddress ), sizeof( "RFID_TAG" ) - NULL_CHARACTER );
		}
		else if( accessMethod == BIOMETRY )
		{
			writtenAddress += StringToBuffer( "BIOMETRY", ( testBuffer + writtenAddress ), sizeof( "BIOMETRY" ) - NULL_CHARACTER );
		}
		else
		{
			writtenAddress += StringToBuffer( "-", ( testBuffer + writtenAddress ), sizeof( "-" ) - NULL_CHARACTER );
		}
		testBuffer[ writtenAddress++ ] = '\n';
		testBuffer[ writtenAddress++ ] = '\r';

		fatfsCode      = f_write(&file, testBuffer, writtenAddress - NULL_CHARACTER, (UINT *)&resultSize);
		if ((fatfsCode) || (resultSize != ( writtenAddress - NULL_CHARACTER ) ) )
		{
			usb_echo("error\r\n");
			f_close(&file);
			USB_HostMsdFatfsTestDone();
			return ERROR;
		}
		fatfsCode = f_sync(&file);
		if (fatfsCode)
		{
			usb_echo("error\r\n");
			f_close(&file);
			USB_HostMsdFatfsTestDone();
			return ERROR;
		}
		countData += BYTES_QUANTITY;
		memset( testBuffer, 0x00, writtenAddress );
	}
	fatfsCode = f_close(&file);
	if (fatfsCode)
	{
		usb_echo("error\r\n");
		USB_HostMsdFatfsTestDone();
		return ERROR;
	}
	return OK;
}

void USB_HostMsdControlCallback(void *param, uint8_t *data, uint32_t dataLength, usb_status_t status)
{
	usb_host_msd_fatfs_instance_t *msdFatfsInstance = (usb_host_msd_fatfs_instance_t *)param;

	if (msdFatfsInstance->runWaitState == kUSB_HostMsdRunWaitSetInterface) /* set interface finish */
	{
		msdFatfsInstance->runWaitState = kUSB_HostMsdRunIdle;
		msdFatfsInstance->runState     = kUSB_HostMsdRunMassStorageTest;
	}
	controlIng    = 0;
	controlStatus = status;
}

static void USB_HostMsdFatfsTestDone(void)
{
	usb_echo("............................test done......................\r\n");
}

#if ((defined MSD_FATFS_THROUGHPUT_TEST_ENABLE) && (MSD_FATFS_THROUGHPUT_TEST_ENABLE))

static void USB_HostMsdFatfsThroughputTest(usb_host_msd_fatfs_instance_t *msdFatfsInstance)
{
	uint64_t totalTime;
	FRESULT fatfsCode;
	FIL file;
	uint32_t resultSize;
	uint32_t testSize;
	uint8_t testIndex;
	char test_file_name[30];

	/* time delay (~100ms) */
	for (resultSize = 0; resultSize < 400000; ++resultSize)
	{
		__NOP();
	}

	usb_echo("............................fatfs test.....................\r\n");
	CoreDebug->DEMCR |= (1 << CoreDebug_DEMCR_TRCENA_Pos);

	for (testSize = 0; testSize < (THROUGHPUT_BUFFER_SIZE / 4); ++testSize)
	{
		testThroughputBuffer[testSize] = testSize;
	}

	sprintf(test_file_name, "%c:", USBDISK + '0');
	fatfsCode = f_mount(&fatfs, test_file_name, 1);
	if (fatfsCode)
	{
		usb_echo("fatfs mount error\r\n");
		USB_HostMsdFatfsTestDone();
		return;
	}

	sprintf(test_file_name, "%c:/thput.dat", USBDISK + '0');
	usb_echo("throughput test:\r\n");
	for (testIndex = 0; testIndex < (sizeof(testSizeArray) / 4); ++testIndex)
	{
		fatfsCode = f_unlink(test_file_name); /* delete the file if it is existed */
		if ((fatfsCode != FR_OK) && (fatfsCode != FR_NO_FILE))
		{
			USB_HostMsdFatfsTestDone();
			return;
		}

		fatfsCode = f_open(&file, test_file_name, FA_WRITE | FA_READ | FA_CREATE_ALWAYS); /* create one new file */
		if (fatfsCode)
		{
			USB_HostMsdFatfsTestDone();
			return;
		}

		totalTime = 0;
		testSize  = testSizeArray[testIndex] * 1024;
		while (testSize)
		{
			if (msdFatfsInstance->deviceState != kStatus_DEV_Attached)
			{
				USB_HostMsdFatfsTestDone();
				return;
			}
			DWT->CYCCNT = 0;
			DWT->CTRL |= (1 << DWT_CTRL_CYCCNTENA_Pos);
			fatfsCode = f_write(&file, testThroughputBuffer, THROUGHPUT_BUFFER_SIZE, &resultSize);
			if (fatfsCode)
			{
				usb_echo("write error\r\n");
				f_close(&file);
				USB_HostMsdFatfsTestDone();
				return;
			}
			totalTime += DWT->CYCCNT;
			DWT->CTRL &= ~(1 << DWT_CTRL_CYCCNTENA_Pos);
			testSize -= THROUGHPUT_BUFFER_SIZE;
		}
		testSize = testSizeArray[testIndex];
		usb_echo("    write %dKB data the speed is %d KB/s\r\n", testSize,
				(uint32_t)((uint64_t)testSize * (uint64_t)MCU_CORE_CLOCK / (uint64_t)totalTime));

		fatfsCode = f_lseek(&file, 0);
		if (fatfsCode)
		{
			USB_HostMsdFatfsTestDone();
			return;
		}
		totalTime = 0;
		testSize  = testSizeArray[testIndex] * 1024;
		while (testSize)
		{
			if (msdFatfsInstance->deviceState != kStatus_DEV_Attached)
			{
				USB_HostMsdFatfsTestDone();
				return;
			}
			DWT->CYCCNT = 0;
			DWT->CTRL |= (1 << DWT_CTRL_CYCCNTENA_Pos);
			fatfsCode = f_read(&file, testThroughputBuffer, THROUGHPUT_BUFFER_SIZE, &resultSize);
			if (fatfsCode)
			{
				usb_echo("read error\r\n");
				f_close(&file);
				USB_HostMsdFatfsTestDone();
				return;
			}
			totalTime += DWT->CYCCNT;
			DWT->CTRL &= ~(1 << DWT_CTRL_CYCCNTENA_Pos);
			testSize -= THROUGHPUT_BUFFER_SIZE;
		}
		testSize = testSizeArray[testIndex];
		usb_echo("    read %dKB data the speed is %d KB/s\r\n", testSize,
				(uint32_t)((uint64_t)testSize * (uint64_t)MCU_CORE_CLOCK / (uint64_t)totalTime));

		fatfsCode = f_close(&file);
		if (fatfsCode)
		{
			USB_HostMsdFatfsTestDone();
			return;
		}
	}

	USB_HostMsdFatfsTestDone();
}

#else

static void USB_HostMsdFatfsDisplayFileInfo(FILINFO *fileInfo)
{
	char *fileName;
	fileName = fileInfo->fname;
	/* note: if this file/directory don't have one attribute, '_' replace the attribute letter ('R' - readonly, 'H' -
	 * hide, 'S' - system) */
	usb_echo("    %s - %c%c%c - %s - %dBytes - %d-%d-%d %d:%d:%d\r\n", (fileInfo->fattrib & AM_DIR) ? "dir" : "fil",
			(fileInfo->fattrib & AM_RDO) ? 'R' : '_', (fileInfo->fattrib & AM_HID) ? 'H' : '_',
					(fileInfo->fattrib & AM_SYS) ? 'S' : '_', fileName, (fileInfo->fsize),
							(uint32_t)((fileInfo->fdate >> 9) + 1980) /* year */,
							(uint32_t)((fileInfo->fdate >> 5) & 0x000Fu) /* month */, (uint32_t)(fileInfo->fdate & 0x001Fu) /* day */,
							(uint32_t)((fileInfo->ftime >> 11) & 0x0000001Fu) /* hour */,
							(uint32_t)((fileInfo->ftime >> 5) & 0x0000003Fu) /* minute */,
							(uint32_t)(fileInfo->ftime & 0x0000001Fu) /* second */
	);
}

static FRESULT USB_HostMsdFatfsListDirectory(const TCHAR *path)
{
	FRESULT fatfsCode = FR_OK;
	FILINFO fileInfo;
	DIR dir;
	uint8_t outputLabel = 0;

	fatfsCode = f_opendir(&dir, path);
	if (fatfsCode)
	{
		return fatfsCode;
	}
	while (1)
	{
		fatfsCode = f_readdir(&dir, &fileInfo);
		if ((fatfsCode) || (!fileInfo.fname[0]))
		{
			break;
		}
		outputLabel = 1;
		USB_HostMsdFatfsDisplayFileInfo(&fileInfo);
	}
	if (!outputLabel)
	{
		usb_echo("\r\n");
	}

	return fatfsCode;
}

#if _USE_FORWARD && _FS_TINY
static uint32_t USB_HostMsdFatfsForward(const uint8_t *data, uint32_t dataLength)
{
	uint32_t resultCount = dataLength;

	if (dataLength == 0)
	{
		return 1;
	}
	else
	{
		do
		{
			usb_echo("%c", *data);
			data++;
			resultCount--;
		} while (resultCount);
		return dataLength;
	}
}
#endif

static void USB_HostMsdFatfsTest(usb_host_msd_fatfs_instance_t *msdFatfsInstance)
{
	FRESULT fatfsCode;
	FATFS *fs;
	FIL file;
	FILINFO fileInfo;
	uint32_t freeClusterNumber;
	uint32_t index;
	uint32_t resultSize;
	char *testString;
	uint8_t driverNumberBuffer[3];

	/* time delay */
	for (freeClusterNumber = 0; freeClusterNumber < 10000; ++freeClusterNumber)
	{
		__NOP();
	}

	usb_echo("............................fatfs test.....................\r\n");

	usb_echo("fatfs mount as logiacal driver %d......", USBDISK);
	sprintf((char *)&driverNumberBuffer[0], "%c:", USBDISK + '0');
	fatfsCode = f_mount(&fatfs, (char const *)&driverNumberBuffer[0], 0);
	if (fatfsCode)
	{
		usb_echo("error\r\n");
		USB_HostMsdFatfsTestDone();
		return;
	}
	usb_echo("success\r\n");

#if (FF_FS_RPATH >= 2)
	fatfsCode = f_chdrive((char const *)&driverNumberBuffer[0]);
	if (fatfsCode)
	{
		usb_echo("error\r\n");
		USB_HostMsdFatfsTestDone();
		return;
	}
#endif
	//
	//#if FF_USE_MKFS
	//    MKFS_PARM formatOptions;
	//    formatOptions.fmt = FM_SFD | FM_ANY;
	//    usb_echo("test f_mkfs......");
	//    fatfsCode = f_mkfs((char const *)&driverNumberBuffer[0], &formatOptions, testBuffer, FF_MAX_SS);
	//    if (fatfsCode)
	//    {
	//        usb_echo("error\r\n");
	//        USB_HostMsdFatfsTestDone();
	//        return;
	//    }
	//    usb_echo("success\r\n");
	//#endif /* FF_USE_MKFS */
	//
	//    usb_echo("test f_getfree:\r\n");
	//    fatfsCode = f_getfree((char const *)&driverNumberBuffer[0], (DWORD *)&freeClusterNumber, &fs);
	//    if (fatfsCode)
	//    {
	//        usb_echo("error\r\n");
	//        USB_HostMsdFatfsTestDone();
	//        return;
	//    }
	//    if (fs->fs_type == FS_FAT12)
	//    {
	//        usb_echo("    FAT type = FAT12\r\n");
	//    }
	//    else if (fs->fs_type == FS_FAT16)
	//    {
	//        usb_echo("    FAT type = FAT16\r\n");
	//    }
	//    else
	//    {
	//        usb_echo("    FAT type = FAT32\r\n");
	//    }
	//    usb_echo("    bytes per cluster = %d; number of clusters=%lu \r\n", fs->csize * 512, fs->n_fatent - 2);
	//    usb_echo("    The free size: %dKB, the total size:%dKB\r\n", (freeClusterNumber * (fs->csize) / 2),
	//             ((fs->n_fatent - 2) * (fs->csize) / 2));
	//
	//    usb_echo("directory operation:\r\n");
	//    usb_echo("list root directory:\r\n");
	//    fatfsCode = USB_HostMsdFatfsListDirectory((char const *)&driverNumberBuffer[0]);
	//    if (fatfsCode)
	//    {
	//        USB_HostMsdFatfsTestDone();
	//        return;
	//    }
	//    usb_echo("create directory \"dir_1\"......");
	//    fatfsCode = f_mkdir(_T("1:/dir_1"));
	//    if (fatfsCode)
	//    {
	//        if (fatfsCode == FR_EXIST)
	//        {
	//            usb_echo("directory exist\r\n");
	//        }
	//        else
	//        {
	//            usb_echo("error\r\n");
	//            USB_HostMsdFatfsTestDone();
	//            return;
	//        }
	//    }
	//    else
	//    {
	//        usb_echo("success\r\n");
	//    }
	//    usb_echo("create directory \"dir_2\"......");
	//    fatfsCode = f_mkdir(_T("1:/dir_2"));
	//    if (fatfsCode)
	//    {
	//        if (fatfsCode == FR_EXIST)
	//        {
	//            usb_echo("directory exist\r\n");
	//        }
	//        else
	//        {
	//            usb_echo("error\r\n");
	//            USB_HostMsdFatfsTestDone();
	//            return;
	//        }
	//    }
	//    else
	//    {
	//        usb_echo("success\r\n");
	//    }
	//    usb_echo("create sub directory \"dir_2/sub_1\"......");
	//    fatfsCode = f_mkdir(_T("1:/dir_1/sub_1"));
	//    if (fatfsCode)
	//    {
	//        if (fatfsCode == FR_EXIST)
	//        {
	//            usb_echo("directory exist\r\n");
	//        }
	//        else
	//        {
	//            usb_echo("error\r\n");
	//            USB_HostMsdFatfsTestDone();
	//            return;
	//        }
	//    }
	//    else
	//    {
	//        usb_echo("success\r\n");
	//    }
	//    usb_echo("list root directory:\r\n");
	//    fatfsCode = USB_HostMsdFatfsListDirectory(_T("1:"));
	//    if (fatfsCode)
	//    {
	//        USB_HostMsdFatfsTestDone();
	//        return;
	//    }
	//    usb_echo("list directory \"dir_1\":\r\n");
	//    fatfsCode = USB_HostMsdFatfsListDirectory(_T("1:/dir_1"));
	//    if (fatfsCode)
	//    {
	//        USB_HostMsdFatfsTestDone();
	//        return;
	//    }
	//    usb_echo("rename directory \"dir_1/sub_1\" to \"dir_1/sub_2\"......");
	//    fatfsCode = f_rename(_T("1:/dir_1/sub_1"), _T("1:/dir_1/sub_2"));
	//    if (fatfsCode)
	//    {
	//        usb_echo("error\r\n");
	//        USB_HostMsdFatfsTestDone();
	//        return;
	//    }
	//    usb_echo("success\r\n");
	//    usb_echo("delete directory \"dir_1/sub_2\"......");
	//    fatfsCode = f_unlink(_T("1:/dir_1/sub_2"));
	//    if (fatfsCode)
	//    {
	//        usb_echo("error\r\n");
	//        USB_HostMsdFatfsTestDone();
	//        return;
	//    }
	//    usb_echo("success\r\n");
	//
	//#if (FF_FS_RPATH >= 2)
	//    usb_echo("get current directory......");
	//    fatfsCode = f_getcwd((TCHAR *)&testBuffer[0], 256);
	//    if (fatfsCode)
	//    {
	//        usb_echo("error\r\n");
	//        USB_HostMsdFatfsTestDone();
	//        return;
	//    }
	//    usb_echo("%s\r\n", testBuffer);
	//    usb_echo("change current directory to \"dir_1\"......");
	//    fatfsCode = f_chdir(_T("dir_1"));
	//    if (fatfsCode)
	//    {
	//        usb_echo("error\r\n");
	//        USB_HostMsdFatfsTestDone();
	//        return;
	//    }
	//    usb_echo("success\r\n");
	//    usb_echo("list current directory:\r\n");
	//    fatfsCode = USB_HostMsdFatfsListDirectory(_T("."));
	//    if (fatfsCode)
	//    {
	//        USB_HostMsdFatfsTestDone();
	//        return;
	//    }
	//    usb_echo("get current directory......");
	//    fatfsCode = f_getcwd((TCHAR *)&testBuffer[0], 256);
	//    if (fatfsCode)
	//    {
	//        usb_echo("error\r\n");
	//        USB_HostMsdFatfsTestDone();
	//        return;
	//    }
	//    usb_echo("%s\r\n", testBuffer);
	//#endif
	//
	//    usb_echo("get directory \"dir_1\" information:\r\n");
	//    fatfsCode = f_stat(_T("1:/dir_1"), &fileInfo);
	//    if (fatfsCode)
	//    {
	//        usb_echo("error\r\n");
	//        USB_HostMsdFatfsTestDone();
	//        return;
	//    }
	//    USB_HostMsdFatfsDisplayFileInfo(&fileInfo);
	//    usb_echo("change \"dir_1\" timestamp to 2015.10.1, 12:30:0......");
	//    fileInfo.fdate = ((2015 - 1980) << 9 | 10 << 5 | 1); /* 2015.10.1 */
	//    fileInfo.ftime = (12 << 11 | 30 << 5);               /* 12:30:00 */
	//    fatfsCode      = f_utime(_T("1:/dir_1"), &fileInfo);
	//    if (fatfsCode)
	//    {
	//        usb_echo("error\r\n");
	//        USB_HostMsdFatfsTestDone();
	//        return;
	//    }
	//    usb_echo("success\r\n");
	//    usb_echo("get directory \"dir_1\" information:\r\n");
	//    fatfsCode = f_stat(_T("1:/dir_1"), &fileInfo);
	//    if (fatfsCode)
	//    {
	//        usb_echo("error\r\n");
	//        USB_HostMsdFatfsTestDone();
	//        return;
	//    }
	//    USB_HostMsdFatfsDisplayFileInfo(&fileInfo);
	//
	//    usb_echo("file operation:\r\n");
	//    usb_echo("create file \"f_1.dat\"......");
    fatfsCode = f_open(&file, _T("1:/ledTeste.hex"), FA_READ | FA_OPEN_ALWAYS);
    char leitura = 0, subt1 = 0, subt2 = 0, flagStart = 0;
    uint32_t buffer2 = 0, contaDados = 0, CRC = 0, somaCRC = 0, endereco;
    char dadoASCII[44], dadoHex[44];

    while(!isLastPackage)
    {
    	while(leitura != '\n')	// Enquanto leitura é diferente de '\n'
    	{
    		fatfsCode = f_read(&file, &leitura, 1, (UINT *)&resultSize);	// leitura recebe 1 byte do arquivo
    		if(leitura != '\n')	// Se leitura é diferente de :,\r,\n
    		{
    			dadoASCII[buffer2] = leitura;	// dadoASCII[buffer] recebe leitura
    			buffer2++;	// Incrementa buffer em 1
    		}
    	}

    	getFileInformation(dadoASCII);
    	buffer2 = 0;
    	leitura = 0;

#if 0
    	for(contaDados = 0; contaDados < buffer2; contaDados = contaDados + 2)	// Para contaDados começando em 0; contDados menor que buffer; contDados soma mais 2
    	{
    		if((dadoASCII[contaDados] >= '0' && dadoASCII[contaDados] <= '9'))
    		{
    			subt1 = 0x30;
    		}
    		else if((dadoASCII[contaDados] >= 'A' && dadoASCII[contaDados] <= 'F'))
    		{
    			subt1 = 0x37;
    		}
    		if((dadoASCII[contaDados + 1] >= '0' && dadoASCII[contaDados + 1] <= '9'))
    		{
    			subt2 = 0x30;
    		}
    		else if((dadoASCII[contaDados + 1] >= 'A' && dadoASCII[contaDados + 1] <= 'F'))
    		{
    			subt2 = 0x37;
    		}
    		dadoHex[contaDados / 2] = ((dadoASCII[contaDados] - subt1) << 4) + dadoASCII[contaDados + 1] - subt2;	// dadoHex[contDados / 2] recebe dadosASCII[contDados] movido 4 bits para a esquerda - 0x30  + dadosASCII[contDados +1]
    		if(contaDados < (buffer2 - 2))	// Se o contador de Dados é menor que o número de elementos do ascii menos 2 (para não selecionar o CRC)
    		{
    			somaCRC += dadoHex[contaDados / 2] & 0xFF;
    		}
    	}
    	CRC = (0x01 + ~(somaCRC)) & 0xFF; // Cálculo CRC (0x01 + !(soma de todos os elementos de dadoHex)
    	if(CRC != (dadoHex[buffer2 / 2 - 1]))	// Se o CRC calculado é diferente do CRC lido
    	{
    		f_lseek(&file, f_tell(&file) - (buffer2 + 3));	// Retorna ao endereço inicial da leitura atual do arquivo
    	}
    	if(!flagStart)	// Se flagStart é zero
    	{
    		flagStart = 1;	//
    		endereco = (dadoHex[buffer2/2 -3] << 24) + (dadoHex[buffer2/2 -2] << 16);	// Endereço recebe a base mais significativa
    	}
    	else
    	{
    		endereco |= (dadoHex[1] << 8) + dadoHex[2];	//dadosASCII[1] movido 8 bits + dadosASCII[0]
    	}

    	buffer2 = 0;	// buffer recebe 0
        leitura = 0;	// leitura recebe 0
        somaCRC = 0;
#endif
    }

    setArquivoGravado(true);
    return;

	fatfsCode = f_unlink(_T("1:/logs.csv"));
	if (fatfsCode)
	{
		usb_echo("No file\r\n");;
	}
	fatfsCode = f_open(&file, _T("1:/logs.csv"), FA_WRITE | FA_CREATE_NEW);
	if (fatfsCode)
	{
		if (fatfsCode == FR_EXIST)
		{
			usb_echo("file exist\r\n");
		}
		else
		{
			usb_echo("error\r\n");
			USB_HostMsdFatfsTestDone();
			return;
		}
	}
	else
	{
		usb_echo("success\r\n");
	}
	usb_echo("test f_write......");


	uint8_t status = FileCSVWrite( ADDRESS, file );
//	usb_echo("success\r\n");
//	usb_echo("test f_printf......");
//	if (f_printf(&file, _T("%s\r\n"), "f_printf test") == EOF)
//	{
//		usb_echo("error\r\n");
//		f_close(&file);
//		USB_HostMsdFatfsTestDone();
//		return;
//	}
//	fatfsCode = f_sync(&file);
//	if (fatfsCode)
//	{
//		usb_echo("error\r\n");
//		f_close(&file);
//		USB_HostMsdFatfsTestDone();
//		return;
//	}
//	usb_echo("success\r\n");
//	usb_echo("test f_puts......");
//	if (f_puts(_T("f_put test\r\n"), &file) == EOF)
//	{
//		usb_echo("error\r\n");
//		f_close(&file);
//		USB_HostMsdFatfsTestDone();
//		return;
//	}
//	fatfsCode = f_sync(&file);
//	if (fatfsCode)
//	{
//		usb_echo("error\r\n");
//		f_close(&file);
//		USB_HostMsdFatfsTestDone();
//		return;
//	}
//	usb_echo("success\r\n");
//	usb_echo("test f_putc......");
//	testString = "f_putc test\r\n";
//	while (*testString)
//	{
//		if (f_putc(*testString, &file) == EOF)
//		{
//			usb_echo("error\r\n");
//			f_close(&file);
//			USB_HostMsdFatfsTestDone();
//			return;
//		}
//		testString++;
//	}
//	fatfsCode = f_sync(&file);
//	if (fatfsCode)
//	{
//		usb_echo("error\r\n");
//		f_close(&file);
//		USB_HostMsdFatfsTestDone();
//		return;
//	}
//	usb_echo("success\r\n");
//	usb_echo("test f_seek......");
//	fatfsCode = f_lseek(&file, 0);
//	if (fatfsCode)
//	{
//		usb_echo("error\r\n");
//		f_close(&file);
//		USB_HostMsdFatfsTestDone();
//		return;
//	}
//	usb_echo("success\r\n");
//	usb_echo("test f_gets......");
//	testString = f_gets((TCHAR *)&testBuffer[0], 10, &file);
//	usb_echo("%s\r\n", testString);
//	usb_echo("test f_read......");
//	fatfsCode = f_read(&file, testBuffer, 10, (UINT *)&resultSize);
//	if (fatfsCode)
//	{
//		usb_echo("error\r\n");
//		f_close(&file);
//		USB_HostMsdFatfsTestDone();
//		return;
//	}
//	testBuffer[resultSize] = 0;
//	usb_echo("%s\r\n", testBuffer);
//#if _USE_FORWARD && _FS_TINY
//	usb_echo("test f_forward......");
//	fatfsCode = f_forward(&file, USB_HostMsdFatfsForward, 10, &resultSize);
//	if (fatfsCode)
//	{
//		usb_echo("error\r\n");
//		f_close(&file);
//		USB_HostMsdFatfsTestDone();
//		return;
//	}
//	usb_echo("\r\n");
//#endif
//	usb_echo("test f_truncate......");
//	fatfsCode = f_truncate(&file);
//	if (fatfsCode)
//	{
//		usb_echo("error\r\n");
//		f_close(&file);
//		USB_HostMsdFatfsTestDone();
//		return;
//	}
//	usb_echo("success\r\n");
//	usb_echo("test f_close......");
//	fatfsCode = f_sync(&file);
//	if (fatfsCode)
//	{
//		usb_echo("error\r\n");
//		f_close(&file);
//		USB_HostMsdFatfsTestDone();
//		return ERROR;
//	}
//	fatfsCode = f_close(&file);
//	if (fatfsCode)
//	{
//		usb_echo("error\r\n");
//		USB_HostMsdFatfsTestDone();
//		return;
//	}
//	usb_echo("success\r\n");
//	usb_echo("get file \"f_1.dat\" information:\r\n");
//	fatfsCode = f_stat(_T("1:/f_1.dat"), &fileInfo);
//	if (fatfsCode)
//	{
//		usb_echo("error\r\n");
//		USB_HostMsdFatfsTestDone();
//		return;
//	}
//	USB_HostMsdFatfsDisplayFileInfo(&fileInfo);
//	usb_echo("change \"f_1.dat\" timestamp to 2015.10.1, 12:30:0......");
//	fileInfo.fdate = ((uint32_t)(2015 - 1980) << 9 | 10 << 5 | 1); /* 2015.10.1 */
//	fileInfo.ftime = (12 << 11 | 30 << 5);                         /* 12:30:00 */
//	fatfsCode      = f_utime(_T("1:/f_1.dat"), &fileInfo);
//	if (fatfsCode)
//	{
//		usb_echo("error\r\n");
//		USB_HostMsdFatfsTestDone();
//		return;
//	}
//	usb_echo("success\r\n");
//	usb_echo("change \"f_1.dat\" to readonly......");
//	fatfsCode = f_chmod(_T("1:/f_1.dat"), AM_RDO, AM_RDO);
//	if (fatfsCode)
//	{
//		usb_echo("error\r\n");
//		USB_HostMsdFatfsTestDone();
//		return;
//	}
//	usb_echo("success\r\n");
//	usb_echo("get file \"f_1.dat\" information:\r\n");
//	fatfsCode = f_stat(_T("1:/f_1.dat"), &fileInfo);
//	if (fatfsCode)
//	{
//		usb_echo("error\r\n");
//		USB_HostMsdFatfsTestDone();
//		return;
//	}
//	USB_HostMsdFatfsDisplayFileInfo(&fileInfo);
//	usb_echo("remove \"f_1.dat\" readonly attribute......");
//	fatfsCode = f_chmod(_T("1:/f_1.dat"), 0, AM_RDO);
//	if (fatfsCode)
//	{
//		usb_echo("error\r\n");
//		USB_HostMsdFatfsTestDone();
//		return;
//	}
//	usb_echo("success\r\n");
//	usb_echo("get file \"f_1.dat\" information:\r\n");
//	fatfsCode = f_stat(_T("1:/f_1.dat"), &fileInfo);
//	if (fatfsCode)
//	{
//		usb_echo("error\r\n");
//		USB_HostMsdFatfsTestDone();
//		return;
//	}
//	USB_HostMsdFatfsDisplayFileInfo(&fileInfo);
//	usb_echo("rename \"f_1.dat\" to \"f_2.dat\"......");
//	fatfsCode = f_rename(_T("1:/f_1.dat"), _T("1:/f_2.dat"));
//	if (fatfsCode)
//	{
//		if (fatfsCode == FR_EXIST)
//		{
//			usb_echo("file exist\r\n");
//		}
//		else
//		{
//			usb_echo("error\r\n");
//			USB_HostMsdFatfsTestDone();
//			return;
//		}
//	}
//	else
//	{
//		usb_echo("success\r\n");
//	}
//	usb_echo("delete \"f_2.dat\"......");
//	fatfsCode = f_unlink(_T("1:/f_2.dat"));
//	if (fatfsCode)
//	{
//		usb_echo("error\r\n");
//		USB_HostMsdFatfsTestDone();
//		return;
//	}
//	usb_echo("success\r\n");

	USB_HostMsdFatfsTestDone();
}

#endif /* MSD_FATFS_THROUGHPUT_TEST_ENABLE */

void USB_HostMsdTask(void *arg)
{
	usb_status_t status;
	usb_host_msd_fatfs_instance_t *msdFatfsInstance = (usb_host_msd_fatfs_instance_t *)arg;

	if (msdFatfsInstance->deviceState != msdFatfsInstance->prevDeviceState)
	{
		msdFatfsInstance->prevDeviceState = msdFatfsInstance->deviceState;
		switch (msdFatfsInstance->deviceState)
		{
		case kStatus_DEV_Idle:
			break;

		case kStatus_DEV_Attached: /* deivce is attached and numeration is done */
			status                = USB_HostMsdInit(msdFatfsInstance->deviceHandle,
					&msdFatfsInstance->classHandle); /* msd class initialization */
			g_UsbFatfsClassHandle = msdFatfsInstance->classHandle;
			if (status != kStatus_USB_Success)
			{
				usb_echo("usb host msd init fail\r\n");
				return;
			}
			msdFatfsInstance->runState = kUSB_HostMsdRunSetInterface;
			break;

		case kStatus_DEV_Detached: /* device is detached */
			msdFatfsInstance->deviceState = kStatus_DEV_Idle;
			msdFatfsInstance->runState    = kUSB_HostMsdRunIdle;
			USB_HostMsdDeinit(msdFatfsInstance->deviceHandle,
					msdFatfsInstance->classHandle); /* msd class de-initialization */
			msdFatfsInstance->classHandle = NULL;

			usb_echo("mass storage device detached\r\n");
			break;

		default:
			break;
		}
	}

	/* run state */
	switch (msdFatfsInstance->runState)
	{
	case kUSB_HostMsdRunIdle:
		break;

	case kUSB_HostMsdRunSetInterface: /* set msd interface */
		msdFatfsInstance->runState     = kUSB_HostMsdRunIdle;
		msdFatfsInstance->runWaitState = kUSB_HostMsdRunWaitSetInterface;
		status = USB_HostMsdSetInterface(msdFatfsInstance->classHandle, msdFatfsInstance->interfaceHandle, 0,
				USB_HostMsdControlCallback, msdFatfsInstance);
		if (status != kStatus_USB_Success)
		{
			usb_echo("set interface fail\r\n");
		}
		break;

	case kUSB_HostMsdRunMassStorageTest: /* set interface succeed */
#if ((defined MSD_FATFS_THROUGHPUT_TEST_ENABLE) && (MSD_FATFS_THROUGHPUT_TEST_ENABLE))
		USB_HostMsdFatfsThroughputTest(msdFatfsInstance); /* test throughput */
#else
		USB_HostMsdFatfsTest(msdFatfsInstance); /* test msd device */
#endif /* MSD_FATFS_THROUGHPUT_TEST_ENABLE */
		msdFatfsInstance->runState = kUSB_HostMsdRunIdle;
		break;

	default:
		break;
	}
}

usb_status_t USB_HostMsdEvent(usb_device_handle deviceHandle,
		usb_host_configuration_handle configurationHandle,
		uint32_t eventCode)
{
	usb_status_t status = kStatus_USB_Success;
	usb_host_configuration_t *configuration;
	uint8_t interfaceIndex;
	usb_host_interface_t *interface;
	uint32_t infoValue = 0U;
	uint8_t id;

	switch (eventCode)
	{
	case kUSB_HostEventAttach:
		/* judge whether is configurationHandle supported */
		configuration = (usb_host_configuration_t *)configurationHandle;
		for (interfaceIndex = 0; interfaceIndex < configuration->interfaceCount; ++interfaceIndex)
		{
			interface = &configuration->interfaceList[interfaceIndex];
			id        = interface->interfaceDesc->bInterfaceClass;
			if (id != USB_HOST_MSD_CLASS_CODE)
			{
				continue;
			}
			id = interface->interfaceDesc->bInterfaceSubClass;
			if ((id != USB_HOST_MSD_SUBCLASS_CODE_UFI) && (id != USB_HOST_MSD_SUBCLASS_CODE_SCSI))
			{
				continue;
			}
			id = interface->interfaceDesc->bInterfaceProtocol;
			if (id != USB_HOST_MSD_PROTOCOL_BULK)
			{
				continue;
			}
			else
			{
				if (g_MsdFatfsInstance.deviceState == kStatus_DEV_Idle)
				{
					/* the interface is supported by the application */
					g_MsdFatfsInstance.deviceHandle    = deviceHandle;
					g_MsdFatfsInstance.interfaceHandle = interface;
					g_MsdFatfsInstance.configHandle    = configurationHandle;
					return kStatus_USB_Success;
				}
				else
				{
					continue;
				}
			}
		}
		status = kStatus_USB_NotSupported;
		break;

	case kUSB_HostEventNotSupported:
		break;

	case kUSB_HostEventEnumerationDone:
		if (g_MsdFatfsInstance.configHandle == configurationHandle)
		{
			if ((g_MsdFatfsInstance.deviceHandle != NULL) && (g_MsdFatfsInstance.interfaceHandle != NULL))
			{
				/* the device enumeration is done */
				if (g_MsdFatfsInstance.deviceState == kStatus_DEV_Idle)
				{
					g_MsdFatfsInstance.deviceState = kStatus_DEV_Attached;

					USB_HostHelperGetPeripheralInformation(deviceHandle, kUSB_HostGetDevicePID, &infoValue);
					usb_echo("mass storage device attached:pid=0x%x", infoValue);
					USB_HostHelperGetPeripheralInformation(deviceHandle, kUSB_HostGetDeviceVID, &infoValue);
					usb_echo("vid=0x%x ", infoValue);
					USB_HostHelperGetPeripheralInformation(deviceHandle, kUSB_HostGetDeviceAddress, &infoValue);
					usb_echo("address=%d\r\n", infoValue);
				}
				else
				{
					usb_echo("not idle msd instance\r\n");
					status = kStatus_USB_Error;
				}
			}
		}
		break;

	case kUSB_HostEventDetach:
		if (g_MsdFatfsInstance.configHandle == configurationHandle)
		{
			/* the device is detached */
			g_UsbFatfsClassHandle           = NULL;
			g_MsdFatfsInstance.configHandle = NULL;
			if (g_MsdFatfsInstance.deviceState != kStatus_DEV_Idle)
			{
				g_MsdFatfsInstance.deviceState = kStatus_DEV_Detached;
			}
		}
		break;

	default:
		break;
	}
	return status;
}

#if ((defined USB_HOST_CONFIG_COMPLIANCE_TEST) && (USB_HOST_CONFIG_COMPLIANCE_TEST))
usb_status_t USB_HostTestEvent(usb_device_handle deviceHandle,
		usb_host_configuration_handle configurationHandle,
		uint32_t eventCode)
{
	/* process the same supported device that is identified by configurationHandle */
	static usb_host_configuration_handle s_ConfigHandle = NULL;
	static usb_device_handle s_DeviceHandle             = NULL;
	static usb_host_interface_handle s_InterfaceHandle  = NULL;
	usb_status_t status                                 = kStatus_USB_Success;
	usb_host_configuration_t *configuration;
	uint8_t interfaceIndex;
	usb_host_interface_t *interface;
	uint32_t id;

	switch (eventCode)
	{
	case kUSB_HostEventAttach:
		/* judge whether is configurationHandle supported */
		configuration = (usb_host_configuration_t *)configurationHandle;
		for (interfaceIndex = 0; interfaceIndex < configuration->interfaceCount; ++interfaceIndex)
		{
			interface = &configuration->interfaceList[interfaceIndex];
			USB_HostHelperGetPeripheralInformation(deviceHandle, kUSB_HostGetDeviceVID, &id);
			if (id == 0x1a0a) /* certification Vendor ID */
					{
				usb_echo("cetification test device VID match\r\n");
				s_DeviceHandle    = deviceHandle;
				s_InterfaceHandle = interface;
				s_ConfigHandle    = configurationHandle;
				return kStatus_USB_Success;
					}
		}
		status = kStatus_USB_NotSupported;
		break;

	case kUSB_HostEventNotSupported:
		usb_echo("Unsupported Device\r\n");
		break;

	case kUSB_HostEventEnumerationDone:
		if (s_ConfigHandle == configurationHandle)
		{
			USB_HostTestModeInit(s_DeviceHandle);
		}
		break;

	case kUSB_HostEventDetach:
		if (s_ConfigHandle == configurationHandle)
		{
			usb_echo("PET test device detach\r\n");
			USB_HostCloseDeviceInterface(s_DeviceHandle, s_InterfaceHandle);
			/* the device is detached */
			s_DeviceHandle    = NULL;
			s_InterfaceHandle = NULL;
			s_ConfigHandle    = NULL;
		}
		break;

	default:
		break;
	}
	return status;
}
#endif
void getFileInformation(char data[])
{
	 static char lastLineQuantBytes[2];
	 static uint8_t quantBytes = 0;
	 uint32_t pageAddr = 0;
	 static uint64_t fullAddrHex = 0;
	 static uint64_t previousAddr = 0;
	 bool finishFirmwareLine = false;
	 uint8_t currentPackageByte = 0;
	 static uint8_t byteCount = 0;
	 static t_protocolSavingFirmwareSteps savingFirmwareStep = STARTER_PACKAGE;

	 if(data[0] == ':')
	 {
		 while(data[currentPackageByte] != '\r')
		 {
			 if(currentPackageByte > 100)
			 {
				 while(1);
				 printf("ERROR");
			 }
			 if(data[currentPackageByte] != '\n')
			 {
				 switch (savingFirmwareStep)
				 {
					case STARTER_PACKAGE:
					{
						if(data[currentPackageByte] == ':')
						{
							firmwareLine.initialByte = data[currentPackageByte];
							savingFirmwareStep = QTD_PACKAGE;
						}
					}
					break;

					case QTD_PACKAGE:
					{
						firmwareLine.quantBytes[byteCount] = data[currentPackageByte];
						byteCount++;

						if(byteCount >= 2)
						{
							byteCount = 0;
							savingFirmwareStep = ADDR_PACKAGE;
						}
					}
					break;

					case ADDR_PACKAGE:
					{
						firmwareLine.addrBytes[byteCount] = data[currentPackageByte];
						byteCount++;

						if(byteCount >= 4)
						{
							memcpy(firmwareLine.fullAddrBytes, firmwareLine.initialAddrBytes, sizeof(firmwareLine.initialAddrBytes));
							memcpy(&firmwareLine.fullAddrBytes[4], firmwareLine.addrBytes, sizeof(firmwareLine.addrBytes));

							if(protocol_getIsFirstAddress())
							{
								protocol_setIsFirstAddress(false);
								previousAddr = protocol_fullAddressHex();
							}
							else
							{
								initialAddress = false;
								fullAddrHex = protocol_fullAddressHex();
							}

							byteCount = 0;
							savingFirmwareStep = CMD_PACKAGE;
						}
					}
					break;

					case CMD_PACKAGE:
					{
						firmwareLine.commandBytes[byteCount] = data[currentPackageByte];
						byteCount++;

						if(byteCount >= 2)
						{
							byteCount = 0;
							savingFirmwareStep = DATA_PACKAGE;

							if(firmwareLine.commandBytes[0] == '0' && firmwareLine.commandBytes[1] == '1')
							{
								isLastPackage = true;
							}
							else if(firmwareLine.commandBytes[0] == '0' && firmwareLine.commandBytes[1] == '4')
							{
								protocol_setIsFirstAddress(true);
								initialAddress = true;
							}
							else if(firmwareLine.commandBytes[0] == '0' && firmwareLine.commandBytes[1] == '5')
							{
								if(((previousAddr + quantBytes) % 256) != 0)
								{
									pageAddr = previousAddr - (previousAddr % 256);
									protocol_setAddressToFirmware(pageAddr);
									_packageFirmwareReady = true;
									isLastPackage = true;
								}
							}
							else
							{
								if((fullAddrHex - previousAddr) > 16 && initialAddress == false)
								{
									/* Pulou mais de um endereço de memória */
									_packageFirmwareReady = true;
									pageAddr = previousAddr - (previousAddr % 256);
									protocol_setAddressToFirmware(pageAddr);
									previousAddr = fullAddrHex;
								}
								else if(initialAddress == false)
								{
									previousAddr = fullAddrHex;
								}
								else
								{
									/* Do something */
								}
							}

						}
					}
					break;

					case DATA_PACKAGE:
					{
						char hex[2];
						hex[0] = firmwareLine.quantBytes[0];
						hex[1] = firmwareLine.quantBytes[1];

						uint8_t dataBytesSize = ((uint8_t)strtol(hex, NULL, 16)) * 2;

						firmwareLine.dataBytes[byteCount] = data[currentPackageByte];

						byteCount++;

						if(byteCount >= dataBytesSize)
						{
							if(protocol_getIsFirstAddress())
							{
								memcpy(firmwareLine.initialAddrBytes, firmwareLine.dataBytes, sizeof(firmwareLine.initialAddrBytes));
							}

							byteCount = 0;
							savingFirmwareStep = CS_PACKAGE;
							if(firmwareLine.quantBytes[0] == '0' && firmwareLine.quantBytes[1] == '0')
							{
								currentPackageByte--;
							}
						}
					}
					break;

					case CS_PACKAGE:
					{
						firmwareLine.checkSumBytes[byteCount] = data[currentPackageByte];
						byteCount++;

						if(byteCount >= 2)
						{
							byteCount = 0;
							finishFirmwareLine = true;
							savingFirmwareStep = STARTER_PACKAGE;
						}
					}
					break;
				 }
			 }

			 if(finishFirmwareLine)
			 {
				 lastLineQuantBytes[0] = firmwareLine.quantBytes[0];
				 lastLineQuantBytes[1] = firmwareLine.quantBytes[1];
				 quantBytes = (uint8_t)strtol(lastLineQuantBytes, NULL, 16);

				 if(firmwareLine.commandBytes[0] == '0' && firmwareLine.commandBytes[1] == '0')
				 {
					 char hex[2];
					 hex[0] = firmwareLine.quantBytes[0];
					 hex[1] = firmwareLine.quantBytes[1];

					 int dataBytesSize = ((int)strtol(hex, NULL, 16));
					 dataBytesSize*= 2;

					 for (uint8_t i = 0; i < dataBytesSize; ++i)
					 {
						 memcpy((uint32_t *)&firmwareData[ctrlFirmwareBuffer.head], (uint32_t *)&firmwareLine.dataBytes[i], sizeof(uint8_t));
						 ctrlFirmwareBuffer.cont++;

						 if (ctrlFirmwareBuffer.head < (512 - 1))
						 {
							 ctrlFirmwareBuffer.head++;
						 }
						 else
						 {
							 ctrlFirmwareBuffer.head = 0;
						 }
					 }

					 if(_packageFirmwareReady == true)
					 {
						 bytesCount = ctrlFirmwareBuffer.cont - dataBytesSize;
					 }
				 }
			 }
			 currentPackageByte++;
	 	 }
	 }

	 if(ctrlFirmwareBuffer.cont >= 512)
	 {
		 ctrlFirmwareBuffer.cont = ctrlFirmwareBuffer.cont - 512;
		 bytesCount = 512;
		 pageAddr = (previousAddr - 16) - ((previousAddr - 16) % 256);
		 if(pageAddr < 0x60400000)
		 {
			 printf("erro/r/n");
		 }
		 protocol_setAddressToFirmware(pageAddr);
		 _packageFirmwareReady = true;
	 }
	 businessRules_updateFirmware();
}

/************************************************************************************************************/
void protocol_setIsFirstAddress (bool firstAddress)
/************************************************************************************************************/
{
	isFirstAddr = firstAddress;
}

/************************************************************************************************************/
bool protocol_getIsFirstAddress (void)
/************************************************************************************************************/
{
	return isFirstAddr;
}

/************************************************************************************************************/
void protocol_setAddressToFirmware (uint32_t address)
/************************************************************************************************************/
{
	addrToFirmware = address;
}

/************************************************************************************************************/
uint32_t protocol_getAddressToFirmware (void)
/************************************************************************************************************/
{
	return addrToFirmware;
}

/************************************************************************************************************/
uint32_t protocol_fullAddressHex(void)
/************************************************************************************************************/
{
	char hex[2];
	uint32_t addrAux = 0;
	uint8_t initialPosition = 24;

	memset(&addrAux, 0, sizeof(addrAux));
	for (uint16_t j = 0; j < sizeof(firmwareLine.fullAddrBytes)/2; j++)
	{
		static uint8_t position = 0;
		hex[0] = firmwareLine.fullAddrBytes[position++];
		hex[1] = firmwareLine.fullAddrBytes[position++];

		uint8_t num = (uint8_t)strtol(hex, NULL, 16);

//		int num = (hex[0] - '0') << 4 | (hex[1] - '0');

		addrAux += num << initialPosition;
		initialPosition = initialPosition - 8;

		if(j == (sizeof(firmwareLine.fullAddrBytes)/2 - 1))
		{
			position = 0;
		}
	}

	if((addrAux < 0x60400000) && !initialAddress)
	{
		printf("erro de endereço/r/n");
	}

	return addrAux;
}

/************************************************************************************************************/
void businessRules_updateFirmware(void)
/************************************************************************************************************/
{
	uint8_t packageBuff[512];
	uint8_t firmwareBuffer[256];

//	HAL_IWDG_Refresh(&hiwdg);

	if(protocol_feedFirmwareBuffer(packageBuff))
	{
		char hex[2];
		uint16_t i = 0;

		memset(firmwareBuffer, 0x00, sizeof(firmwareBuffer));
		for (uint16_t j = 0; j < sizeof(firmwareBuffer); j++)
		{
			hex[0] = packageBuff[i++];
			hex[1] = packageBuff[i++];

			int num = ( (int)strtol(hex, NULL, 16) ) >> 8;
			firmwareBuffer[j] = num;
		}
		businessRules_saveFirmware((uint8_t *)firmwareBuffer);
	}
}

/************************************************************************************************************/
bool protocol_feedFirmwareBuffer(uint8_t * buffDest)
/************************************************************************************************************/
{
	bool dataIsReady = false;
	uint16_t initialPacket = 0;
	uint16_t finalPacket = 0;
	memset(buffDest, 0x00, 256);

	if(_packageFirmwareReady)
	{
		_packageFirmwareReady = false;
		dataIsReady = true;
		if(!isLastPackage)
		{
			if ((ctrlFirmwareBuffer.tail + bytesCount) == 512)
			{
				memcpy(buffDest, (firmwareData + ctrlFirmwareBuffer.tail), bytesCount);
				ctrlFirmwareBuffer.tail = 0;
			}
			else if ((ctrlFirmwareBuffer.tail + bytesCount) > 512)
			{
				initialPacket = 512 - ctrlFirmwareBuffer.tail;
				finalPacket = bytesCount - initialPacket;
				memcpy(buffDest, (firmwareData + ctrlFirmwareBuffer.tail), initialPacket);
				ctrlFirmwareBuffer.tail = 0;
				memcpy(&buffDest[initialPacket], (firmwareData + ctrlFirmwareBuffer.tail), finalPacket);
				ctrlFirmwareBuffer.tail += finalPacket;
			}
			else
			{
				memcpy(buffDest, (firmwareData + ctrlFirmwareBuffer.tail), bytesCount);
				ctrlFirmwareBuffer.tail += bytesCount;
			}
		}
		else
		{
			memcpy(buffDest, (firmwareData + ctrlFirmwareBuffer.tail), ctrlFirmwareBuffer.cont);
			isLastPackage = false;
			ctrlFirmwareBuffer.cont = 0;
			ctrlFirmwareBuffer.tail = 0;
			ctrlFirmwareBuffer.head = 0;
		}
	}

	return dataIsReady;
}

/************************************************************************************************************/
void businessRules_saveFirmware(uint8_t * buffSource)
/************************************************************************************************************/
{
	addrToSaveFirmware = protocol_getAddressToFirmware();
	FLASH_writeData(addrToSaveFirmware - 0x60000000, (uint8_t *)buffSource, 256);
}

/************************************************************************************************************/
void FLASH_writeData(uint32_t dataAddress, uint8_t * dataToWrite, size_t dataSize)
/************************************************************************************************************/
{
	uint32_t dataPageAddr;
	uint32_t dataSectorAddr;
	uint8_t sectorData[SECTOR_SIZE + FLASH_PAGE_SIZE];
	uint8_t pageData[FLASH_PAGE_SIZE * 2];
	static uint8_t s_nor_program_buffer[256];

	dataPageAddr = FLASH_checkWhichAddrPage(dataAddress);
	FLASH_readData((uint32_t *)&pageData, EXAMPLE_FLEXSPI_AMBA_BASE + dataPageAddr, sizeof(pageData));
	memcpy(pageData + (dataAddress - dataPageAddr), dataToWrite, dataSize);

	dataSectorAddr = FLASH_checkWhichAddrSector(dataPageAddr);
	FLASH_readData((uint32_t *)&sectorData, EXAMPLE_FLEXSPI_AMBA_BASE + dataSectorAddr, SECTOR_SIZE);
	memcpy(sectorData + (dataPageAddr - dataSectorAddr), pageData, sizeof(pageData));


#warning "as funções DisableGlobalIRQ e EnableGlobalIRQ(maskInt) serão substituídas pela função taskENTER_CRITICAL"
    uint32_t maskInt = DisableGlobalIRQ();

    flexspi_nor_enable_quad_mode(EXAMPLE_FLEXSPI);

    static int contSector;

	flexspi_nor_flash_erase_sector(EXAMPLE_FLEXSPI, dataSectorAddr );

	DCACHE_InvalidateByRange(EXAMPLE_FLEXSPI_AMBA_BASE + dataSectorAddr, FLASH_PAGE_SIZE);

	for(uint8_t i = 0 ; i < SECTOR_SIZE / FLASH_PAGE_SIZE; i++)
	{
		flexspi_nor_flash_page_program(EXAMPLE_FLEXSPI, dataSectorAddr + (i * FLASH_PAGE_SIZE), (void *)(sectorData + (i * FLASH_PAGE_SIZE)));
	}

	EnableGlobalIRQ(maskInt);

	DCACHE_InvalidateByRange(EXAMPLE_FLEXSPI_AMBA_BASE + dataSectorAddr, FLASH_PAGE_SIZE);
}

uint32_t FLASH_checkWhichAddrPage (uint32_t addressToCheck)
{
	uint32_t pageAddr;

	pageAddr = addressToCheck - (addressToCheck % 256);

	return pageAddr;
}

uint32_t FLASH_checkWhichAddrSector (uint32_t addressToCheck)
{
	uint32_t sectorAddr;

	sectorAddr = addressToCheck - (addressToCheck % 4096);

	return sectorAddr;
}

void FLASH_readData(uint32_t * data, uint32_t dataAddr, uint32_t dataSize)
{
//	taskENTER_CRITICAL();
	memcpy(data, (void *)(dataAddr), dataSize);
//	taskEXIT_CRITICAL();
}

void setArquivoGravado(bool status)
{
	flag = status;
}

bool getArquivoGravado(void)
{
	return flag;
}
