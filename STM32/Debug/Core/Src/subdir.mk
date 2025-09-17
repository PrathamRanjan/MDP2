################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/ICM20948.c \
../Core/Src/ICM20948_SelfTest.c \
../Core/Src/angle.c \
../Core/Src/commands.c \
../Core/Src/convert.c \
../Core/Src/delay_us.c \
../Core/Src/display_status.c \
../Core/Src/dist.c \
../Core/Src/kalman.c \
../Core/Src/mag_cal.c \
../Core/Src/main.c \
../Core/Src/motion.c \
../Core/Src/motor.c \
../Core/Src/pid.c \
../Core/Src/sensor_test.c \
../Core/Src/sensors.c \
../Core/Src/servo.c \
../Core/Src/stm32f4xx_hal_msp.c \
../Core/Src/stm32f4xx_it.c \
../Core/Src/syscalls.c \
../Core/Src/sysmem.c \
../Core/Src/system_stm32f4xx.c \
../Core/Src/tim6_pulse_capt.c \
../Core/Src/user_input.c 

OBJS += \
./Core/Src/ICM20948.o \
./Core/Src/ICM20948_SelfTest.o \
./Core/Src/angle.o \
./Core/Src/commands.o \
./Core/Src/convert.o \
./Core/Src/delay_us.o \
./Core/Src/display_status.o \
./Core/Src/dist.o \
./Core/Src/kalman.o \
./Core/Src/mag_cal.o \
./Core/Src/main.o \
./Core/Src/motion.o \
./Core/Src/motor.o \
./Core/Src/pid.o \
./Core/Src/sensor_test.o \
./Core/Src/sensors.o \
./Core/Src/servo.o \
./Core/Src/stm32f4xx_hal_msp.o \
./Core/Src/stm32f4xx_it.o \
./Core/Src/syscalls.o \
./Core/Src/sysmem.o \
./Core/Src/system_stm32f4xx.o \
./Core/Src/tim6_pulse_capt.o \
./Core/Src/user_input.o 

C_DEPS += \
./Core/Src/ICM20948.d \
./Core/Src/ICM20948_SelfTest.d \
./Core/Src/angle.d \
./Core/Src/commands.d \
./Core/Src/convert.d \
./Core/Src/delay_us.d \
./Core/Src/display_status.d \
./Core/Src/dist.d \
./Core/Src/kalman.d \
./Core/Src/mag_cal.d \
./Core/Src/main.d \
./Core/Src/motion.d \
./Core/Src/motor.d \
./Core/Src/pid.d \
./Core/Src/sensor_test.d \
./Core/Src/sensors.d \
./Core/Src/servo.d \
./Core/Src/stm32f4xx_hal_msp.d \
./Core/Src/stm32f4xx_it.d \
./Core/Src/syscalls.d \
./Core/Src/sysmem.d \
./Core/Src/system_stm32f4xx.d \
./Core/Src/tim6_pulse_capt.d \
./Core/Src/user_input.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/%.o Core/Src/%.su Core/Src/%.cyclo: ../Core/Src/%.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F407xx -c -I../PeripheralDrivers/Inc -I../Core/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Src

clean-Core-2f-Src:
	-$(RM) ./Core/Src/ICM20948.cyclo ./Core/Src/ICM20948.d ./Core/Src/ICM20948.o ./Core/Src/ICM20948.su ./Core/Src/ICM20948_SelfTest.cyclo ./Core/Src/ICM20948_SelfTest.d ./Core/Src/ICM20948_SelfTest.o ./Core/Src/ICM20948_SelfTest.su ./Core/Src/angle.cyclo ./Core/Src/angle.d ./Core/Src/angle.o ./Core/Src/angle.su ./Core/Src/commands.cyclo ./Core/Src/commands.d ./Core/Src/commands.o ./Core/Src/commands.su ./Core/Src/convert.cyclo ./Core/Src/convert.d ./Core/Src/convert.o ./Core/Src/convert.su ./Core/Src/delay_us.cyclo ./Core/Src/delay_us.d ./Core/Src/delay_us.o ./Core/Src/delay_us.su ./Core/Src/display_status.cyclo ./Core/Src/display_status.d ./Core/Src/display_status.o ./Core/Src/display_status.su ./Core/Src/dist.cyclo ./Core/Src/dist.d ./Core/Src/dist.o ./Core/Src/dist.su ./Core/Src/kalman.cyclo ./Core/Src/kalman.d ./Core/Src/kalman.o ./Core/Src/kalman.su ./Core/Src/mag_cal.cyclo ./Core/Src/mag_cal.d ./Core/Src/mag_cal.o ./Core/Src/mag_cal.su ./Core/Src/main.cyclo ./Core/Src/main.d ./Core/Src/main.o ./Core/Src/main.su ./Core/Src/motion.cyclo ./Core/Src/motion.d ./Core/Src/motion.o ./Core/Src/motion.su ./Core/Src/motor.cyclo ./Core/Src/motor.d ./Core/Src/motor.o ./Core/Src/motor.su ./Core/Src/pid.cyclo ./Core/Src/pid.d ./Core/Src/pid.o ./Core/Src/pid.su ./Core/Src/sensor_test.cyclo ./Core/Src/sensor_test.d ./Core/Src/sensor_test.o ./Core/Src/sensor_test.su ./Core/Src/sensors.cyclo ./Core/Src/sensors.d ./Core/Src/sensors.o ./Core/Src/sensors.su ./Core/Src/servo.cyclo ./Core/Src/servo.d ./Core/Src/servo.o ./Core/Src/servo.su ./Core/Src/stm32f4xx_hal_msp.cyclo ./Core/Src/stm32f4xx_hal_msp.d ./Core/Src/stm32f4xx_hal_msp.o ./Core/Src/stm32f4xx_hal_msp.su ./Core/Src/stm32f4xx_it.cyclo ./Core/Src/stm32f4xx_it.d ./Core/Src/stm32f4xx_it.o ./Core/Src/stm32f4xx_it.su ./Core/Src/syscalls.cyclo ./Core/Src/syscalls.d ./Core/Src/syscalls.o ./Core/Src/syscalls.su ./Core/Src/sysmem.cyclo ./Core/Src/sysmem.d ./Core/Src/sysmem.o ./Core/Src/sysmem.su ./Core/Src/system_stm32f4xx.cyclo ./Core/Src/system_stm32f4xx.d ./Core/Src/system_stm32f4xx.o ./Core/Src/system_stm32f4xx.su ./Core/Src/tim6_pulse_capt.cyclo ./Core/Src/tim6_pulse_capt.d ./Core/Src/tim6_pulse_capt.o ./Core/Src/tim6_pulse_capt.su ./Core/Src/user_input.cyclo ./Core/Src/user_input.d ./Core/Src/user_input.o ./Core/Src/user_input.su

.PHONY: clean-Core-2f-Src

