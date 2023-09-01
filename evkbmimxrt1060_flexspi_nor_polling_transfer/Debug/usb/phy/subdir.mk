################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../usb/phy/usb_phy.c 

C_DEPS += \
./usb/phy/usb_phy.d 

OBJS += \
./usb/phy/usb_phy.o 


# Each subdirectory must supply rules for building sources it contributes
usb/phy/%.o: ../usb/phy/%.c usb/phy/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: MCU C Compiler'
	arm-none-eabi-gcc -std=gnu99 -D__REDLIB__ -DCPU_MIMXRT1062DVL6A -DCPU_MIMXRT1062DVL6A_cm7 -DSDK_DEBUGCONSOLE=1 -DXIP_EXTERNAL_FLASH=1 -DXIP_BOOT_HEADER_ENABLE=1 -DMCUXPRESSO_SDK -DCR_INTEGER_PRINTF -DPRINTF_FLOAT_ENABLE=0 -D__MCUXPRESSO -D__USE_CMSIS -DDEBUG -I"C:\Users\PDS-User\Documents\MCUXpressoIDE_11.8.0_1165\workspace\evkbmimxrt1060_flexspi_nor_polling_transfer\source" -I"C:\Users\PDS-User\Documents\MCUXpressoIDE_11.8.0_1165\workspace\evkbmimxrt1060_flexspi_nor_polling_transfer\component\osa" -I"C:\Users\PDS-User\Documents\MCUXpressoIDE_11.8.0_1165\workspace\evkbmimxrt1060_flexspi_nor_polling_transfer\fatfs" -I"C:\Users\PDS-User\Documents\MCUXpressoIDE_11.8.0_1165\workspace\evkbmimxrt1060_flexspi_nor_polling_transfer\usb" -I"C:\Users\PDS-User\Documents\MCUXpressoIDE_11.8.0_1165\workspace\evkbmimxrt1060_flexspi_nor_polling_transfer\usb\host" -I"C:\Users\PDS-User\Documents\MCUXpressoIDE_11.8.0_1165\workspace\evkbmimxrt1060_flexspi_nor_polling_transfer\usb\host\class" -I"C:\Users\PDS-User\Documents\MCUXpressoIDE_11.8.0_1165\workspace\evkbmimxrt1060_flexspi_nor_polling_transfer\usb\include" -I"C:\Users\PDS-User\Documents\MCUXpressoIDE_11.8.0_1165\workspace\evkbmimxrt1060_flexspi_nor_polling_transfer\usb\phy" -I"C:\Users\PDS-User\Documents\MCUXpressoIDE_11.8.0_1165\workspace\evkbmimxrt1060_flexspi_nor_polling_transfer\fatfs\source" -I"C:\Users\PDS-User\Documents\MCUXpressoIDE_11.8.0_1165\workspace\evkbmimxrt1060_flexspi_nor_polling_transfer\fatfs\source\fsl_ram_disk" -I"C:\Users\PDS-User\Documents\MCUXpressoIDE_11.8.0_1165\workspace\evkbmimxrt1060_flexspi_nor_polling_transfer\fatfs\source\fsl_usb_disk" -I"C:\Users\PDS-User\Documents\MCUXpressoIDE_11.8.0_1165\workspace\evkbmimxrt1060_flexspi_nor_polling_transfer\utilities" -I"C:\Users\PDS-User\Documents\MCUXpressoIDE_11.8.0_1165\workspace\evkbmimxrt1060_flexspi_nor_polling_transfer\drivers" -I"C:\Users\PDS-User\Documents\MCUXpressoIDE_11.8.0_1165\workspace\evkbmimxrt1060_flexspi_nor_polling_transfer\device" -I"C:\Users\PDS-User\Documents\MCUXpressoIDE_11.8.0_1165\workspace\evkbmimxrt1060_flexspi_nor_polling_transfer\component\uart" -I"C:\Users\PDS-User\Documents\MCUXpressoIDE_11.8.0_1165\workspace\evkbmimxrt1060_flexspi_nor_polling_transfer\component\lists" -I"C:\Users\PDS-User\Documents\MCUXpressoIDE_11.8.0_1165\workspace\evkbmimxrt1060_flexspi_nor_polling_transfer\xip" -I"C:\Users\PDS-User\Documents\MCUXpressoIDE_11.8.0_1165\workspace\evkbmimxrt1060_flexspi_nor_polling_transfer\CMSIS" -I"C:\Users\PDS-User\Documents\MCUXpressoIDE_11.8.0_1165\workspace\evkbmimxrt1060_flexspi_nor_polling_transfer\board" -O0 -fno-common -g3 -c -ffunction-sections -fdata-sections -ffreestanding -fno-builtin -fmerge-constants -fmacro-prefix-map="$(<D)/"= -mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -D__REDLIB__ -fstack-usage -specs=redlib.specs -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.o)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


clean: clean-usb-2f-phy

clean-usb-2f-phy:
	-$(RM) ./usb/phy/usb_phy.d ./usb/phy/usb_phy.o

.PHONY: clean-usb-2f-phy

