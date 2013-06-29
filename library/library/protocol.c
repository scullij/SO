/*
 * protocol.c
 *
 *  Created on: 29/04/2013
 *      Author: utnso
 */

#include "protocol.h"
#include <stdlib.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

int16_t recibir(int16_t socket, void** payload){
	int16_t recv1;
	header_t *header = malloc(sizeof(header_t));

	if((recv1 = recv(socket, header, sizeof(header), MSG_WAITALL))<=0)
	{
		if (recv1==0){
			return -1;//CAYO_SERVIDOR
		}else{
			perror("recv-badheader");
			return -2;//MALA_RECEPCION
		}
	}else{
		if(header->length <= 0){
			return header->type;
		}

		*payload = malloc(header->length);

		int16_t bytesRecibidos;
		if((bytesRecibidos=recv(socket,*payload,header->length,MSG_WAITALL))==-1){
			free(*payload);
			perror("recv-badbuffer");
			return -4;//MALA_RECEPCION
		}

	}

	return header->type;
}

int16_t enviar(int16_t socket, int16_t tipoEnvio, void* payload, uint16_t payloadLenght){
	header_t *header = malloc(sizeof(header_t));
	header->type = tipoEnvio;
	header->length = payloadLenght;

	int16_t send1;
	char* buffer = malloc(sizeof(header_t) + payloadLenght);
	memcpy(buffer, header, sizeof(header_t));
	if(payload != NULL){
		memcpy(buffer + sizeof(header_t), payload, payloadLenght);
	}

	if ((send1 = send(socket, buffer, sizeof(header_t) + payloadLenght,0)) == -1)
	{
		perror("Error al enviar datos.");
		return send1;
	}

	free(buffer);
	free(header);

	return 0;
}
