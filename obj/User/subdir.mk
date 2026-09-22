################################################################################
# MRS Version: 2.5.0
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../User/bsp_adc.c \
../User/bsp_isp.c \
../User/bsp_key.c \
../User/bsp_led.c \
../User/bsp_pwm.c \
../User/ch32v20x_it.c \
../User/gunlight.c \
../User/main.c \
../User/system_ch32v20x.c 

C_DEPS += \
./User/bsp_adc.d \
./User/bsp_isp.d \
./User/bsp_key.d \
./User/bsp_led.d \
./User/bsp_pwm.d \
./User/ch32v20x_it.d \
./User/gunlight.d \
./User/main.d \
./User/system_ch32v20x.d 

OBJS += \
./User/bsp_adc.o \
./User/bsp_isp.o \
./User/bsp_key.o \
./User/bsp_led.o \
./User/bsp_pwm.o \
./User/ch32v20x_it.o \
./User/gunlight.o \
./User/main.o \
./User/system_ch32v20x.o 

DIR_OBJS += \
./User/*.o \

DIR_DEPS += \
./User/*.d \

DIR_EXPANDS += \
./User/*.234r.expand \


# Each subdirectory must supply rules for building sources it contributes
User/%.o: ../User/%.c
	@	riscv-none-embed-gcc -march=rv32imacxw -mabi=ilp32 -msmall-data-limit=8 -msave-restore -fmax-errors=20 -Os -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-common -Wunused -Wuninitialized -g -I"c:/Users/Origin/mounriver-studio-projects/CH32V203F8U/Debug" -I"c:/Users/Origin/mounriver-studio-projects/CH32V203F8U/Core" -I"c:/Users/Origin/mounriver-studio-projects/CH32V203F8U/User" -I"c:/Users/Origin/mounriver-studio-projects/CH32V203F8U/Peripheral/inc" -std=gnu99 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"

