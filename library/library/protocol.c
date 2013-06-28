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

int16_t recibir(int16_t socket, char** p_cadena){
	int16_t recv1;
	header_t *header = malloc(sizeof(header_t));

	if((recv1 = recv(socket, &(header->type), sizeof(header->type), MSG_WAITALL))<=0)
	{
		if (recv1==0){
			return -1;//CAYO_SERVIDOR
		}else{
			perror("recv-badheader");
			return -2;//MALA_RECEPCION
		}
	}else{
		int16_t recv2;
		if((recv2 = recv(socket,&header->length,sizeof(header->length),MSG_WAITALL))<=0){
			perror("recv-badlenght");
			return -3;//MALA_RECEPCION
		}else{
			*p_cadena=malloc(header->length);

			int16_t bytesRecibidos;
			if((bytesRecibidos=recv(socket,*p_cadena,header->length,MSG_WAITALL))==-1){
				free(*p_cadena);
				perror("recv-badbuffer");
				return -4;//MALA_RECEPCION
			}
		}

	}

	return header->type;
}

int16_t enviar(int16_t socket, char* cadena, int16_t tipoEnvio){
	header_t *header = malloc(sizeof(header_t));
	header->type = tipoEnvio;
	header->length = strlen(cadena)+1;
	int16_t send1;
	char* cadenaAEnviar = malloc(sizeof(header_t) + header->length);
	memcpy(cadenaAEnviar, header, sizeof(header_t));
	memcpy(cadenaAEnviar + sizeof(header_t), cadena, header->length);

	if ((send1 = send(socket, cadenaAEnviar, header->length + sizeof(header_t),0)) == -1)
	{
		perror("Error al enviar datos.");
		return send1;
	}

	free(cadenaAEnviar);
	free(header);

	return 0;
}
