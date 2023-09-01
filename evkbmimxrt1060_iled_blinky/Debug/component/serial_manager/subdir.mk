################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../component/serial_manager/fsl_component_serial_manager.c \
../component/serial_manager/fsl_component_serial_port_uart.c 

C_DEPS += \
./component/serial_manager/fsl_component_serial_manager.d \
./component/serial_manager/fsl_component_serial_port_uart.d 

OBJS += \
./component/serial_manager/fsl_component_serial_manager.o \
./component/serial_manager/fsl_component_serial_port_uart.o 


# Each subdirectory must supply rules for building sources it contributes
component/serial_manager/%.o: ../component/serial_manager/%.c component/serial_manager/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: MCU C Compiler'
	arm-none-eabi-gcc -std=gnu99 -D__REDLIB__ -DCPU_MIMXRT1062DVL6B -DCPU_MIMXRT1062DVL6B_cm7 -DSDK_DEBUGCONSOLE=1 -DXIP_EXTERNAL_FLASH=1 -DXIP_BOOT_HEADER_ENABLE=1 -DMCUXPRESSO_SDK -DSERIAL_PORT_TYPE_UART=1 -DCR_INTEGER_PRINTF -DPRINTF_FLOAT_ENABLE=0 -D__MCUXPRESSO -D__USE_CMSIS -DDEBUG -I"C:\Users\PDS-User\Documents\MCUXpressoIDE_11.8.0_1165\workspace\evkbmimxrt1060_iled_blinky\source" -I"C:\Users\PDS-User\Documents\MCUXpressoIDE_11.8.0_1165\workspace\evkbmimxrt1060_iled_blinky\drivers" -I"C:\Users\PDS-User\Documents\MCUXpressoIDE_11.8.0_1165\workspace\evkbmimxrt1060_iled_blinky\device" -I"C:\Users\PDS-User\Documents\MCUXpressoIDE_11.8.0_1165\workspace\evkbmimxrt1060_iled_blinky\utilities" -I"C:\Users\PDS-User\Documents\MCUXpressoIDE_11.8.0_1165\workspace\evkbmimxrt1060_iled_blinky\component\uart" -I"C:\Users\PDS-User\Documents\MCUXpressoIDE_11.8.0_1165\workspace\evkbmimxrt1060_iled_blinky\component\serial_manager" -I"C:\Users\PDS-User\Documents\MCUXpressoIDE_11.8.0_1165\workspace\evkbmimxrt1060_iled_blinky\component\lists" -I"C:\Users\PDS-User\Documents\MCUXpressoIDE_11.8.0_1165\workspace\evkbmimxrt1060_iled_blinky\xip" -I"C:\Users\PDS-User\Documents\MCUXpressoIDE_11.8.0_1165\workspace\evkbmimxrt1060_iled_blinky\CMSIS" -I"C:\Users\PDS-User\Documents\MCUXpressoIDE_11.8.0_1165\workspace\evkbmimxrt1060_iled_blinky\board" -O0 -fno-common -g3 -c -ffunction-sections -fdata-sections -ffreestanding -fno-builtin -fmerge-constants -fmacro-prefix-map="$(<D)/"= -mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -D__REDLIB__ -fstack-usage -specs=redlib.specs -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.o)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


clean: clean-component-2f-serial_manager

clean-component-2f-serial_manager:
	-$(RM) ./component/serial_manager/fsl_component_serial_manager.d ./component/serial_manager/fsl_component_serial_manager.o ./component/serial_manager/fsl_component_serial_port_uart.d ./component/serial_manager/fsl_component_serial_port_uart.o

.PHONY: clean-component-2f-serial_manager

