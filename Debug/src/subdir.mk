################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/main.c \
../src/personaje.c \
../src/plataforma.c \
../src/protocol.c \
../src/socket.c 

OBJS += \
./src/main.o \
./src/personaje.o \
./src/plataforma.o \
./src/protocol.o \
./src/socket.o 

C_DEPS += \
./src/main.d \
./src/personaje.d \
./src/plataforma.d \
./src/protocol.d \
./src/socket.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -I"/home/utnso/workspace/so-commons-library" -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


