################################################################################
# MRS Version: 2.5.0
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../User/bsp_adc.c \
../User/bsp_cli.c \
../User/bsp_isp.c \
../User/bsp_key.c \
../User/bsp_led.c \
../User/bsp_pwm.c \
../User/ch32v20x_it.c \
../User/gunlight.c \
../User/main.c \
../User/system_ch32v20x.c \
../User/usb_cdc.c \
../User/USBLIB/USB-Driver/src/usb_core.c \
../User/USBLIB/USB-Driver/src/usb_init.c \
../User/USBLIB/USB-Driver/src/usb_int.c \
../User/USBLIB/USB-Driver/src/usb_mem.c \
../User/USBLIB/USB-Driver/src/usb_regs.c \
../User/USBLIB/USB-Driver/src/usb_sil.c \
../User/USBLIB/CONFIG/hw_config.c \
../User/USBLIB/CONFIG/usb_desc.c \
../User/USBLIB/CONFIG/usb_endp.c \
../User/USBLIB/CONFIG/usb_istr.c \
../User/USBLIB/CONFIG/usb_prop.c \
../User/USBLIB/CONFIG/usb_pwr.c 

C_DEPS += \
./User/bsp_adc.d \
./User/bsp_cli.d \
./User/bsp_isp.d \
./User/bsp_key.d \
./User/bsp_led.d \
./User/bsp_pwm.d \
./User/ch32v20x_it.d \
./User/gunlight.d \
./User/main.d \
./User/system_ch32v20x.d \
./User/usb_cdc.d \
./User/usb_core.d \
./User/usb_init.d \
./User/usb_int.d \
./User/usb_mem.d \
./User/usb_regs.d \
./User/usb_sil.d \
./User/hw_config.d \
./User/usb_desc.d \
./User/usb_endp.d \
./User/usb_istr.d \
./User/usb_prop.d \
./User/usb_pwr.d 

OBJS += \
./User/bsp_adc.o \
./User/bsp_cli.o \
./User/bsp_isp.o \
./User/bsp_key.o \
./User/bsp_led.o \
./User/bsp_pwm.o \
./User/ch32v20x_it.o \
./User/gunlight.o \
./User/main.o \
./User/system_ch32v20x.o \
./User/usb_cdc.o \
./User/usb_core.o \
./User/usb_init.o \
./User/usb_int.o \
./User/usb_mem.o \
./User/usb_regs.o \
./User/usb_sil.o \
./User/hw_config.o \
./User/usb_desc.o \
./User/usb_endp.o \
./User/usb_istr.o \
./User/usb_prop.o \
./User/usb_pwr.o 

DIR_OBJS += \
./User/*.o 

DIR_DEPS += \
./User/*.d 

DIR_EXPANDS += \
./User/*.234r.expand 

INC_FLAGS = -I"c:/Users/Origin/mounriver-studio-projects/CH32V203F8U/Debug" \
            -I"c:/Users/Origin/mounriver-studio-projects/CH32V203F8U/Core" \
            -I"c:/Users/Origin/mounriver-studio-projects/CH32V203F8U/User" \
            -I"c:/Users/Origin/mounriver-studio-projects/CH32V203F8U/Peripheral/inc" \
            -I"c:/Users/Origin/mounriver-studio-projects/CH32V203F8U/User/USBLIB/USB-Driver/inc" \
            -I"c:/Users/Origin/mounriver-studio-projects/CH32V203F8U/User/USBLIB/CONFIG"

# Each subdirectory must supply rules for building sources it contributes
User/%.o: ../User/%.c
	@	riscv-none-embed-gcc -march=rv32imacxw -mabi=ilp32 -msmall-data-limit=8 -msave-restore -fmax-errors=20 -Os -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-common -Wunused -Wuninitialized -g $(INC_FLAGS) -std=gnu99 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"

User/%.o: ../User/USBLIB/USB-Driver/src/%.c
	@	riscv-none-embed-gcc -march=rv32imacxw -mabi=ilp32 -msmall-data-limit=8 -msave-restore -fmax-errors=20 -Os -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-common -Wunused -Wuninitialized -g $(INC_FLAGS) -std=gnu99 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"

User/%.o: ../User/USBLIB/CONFIG/%.c
	@	riscv-none-embed-gcc -march=rv32imacxw -mabi=ilp32 -msmall-data-limit=8 -msave-restore -fmax-errors=20 -Os -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-common -Wunused -Wuninitialized -g $(INC_FLAGS) -std=gnu99 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"
