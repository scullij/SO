################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../library/Items.c \
../library/gui.c \
../library/protocol.c \
../library/socket.c 

OBJS += \
./library/Items.o \
./library/gui.o \
./library/protocol.o \
./library/socket.o 

C_DEPS += \
./library/Items.d \
./library/gui.d \
./library/protocol.d \
./library/socket.d 


# Each subdirectory must supply rules for building sources it contributes
library/%.o: ../library/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -fPIC -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


