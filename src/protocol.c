/*
 * protocol.c
 *
 *  Created on: 29/04/2013
 *      Author: utnso
 */

#include <stdlib.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdint.h>

typedef struct {
	int8_t type;
	int16_t length;
} header_t;

int8_t recv_variable(int8_t socketReceptor, void* buffer) {

	header_t header;
	int8_t bytesRecibidos;

	// Primero: Recibir el header para saber cuando ocupa el payload.
	if (recv(socketReceptor, &header, sizeof(header), MSG_WAITALL) <= 0)
		return -1;

	// Segundo: Alocar memoria suficiente para el payload.
	buffer = malloc(header.length);

	// Tercero: Recibir el payload.
	if((bytesRecibidos = recv(socketReceptor, buffer, header.length, MSG_WAITALL)) < 0){
		free(buffer);
		return -1;
	}

	return bytesRecibidos;

}
