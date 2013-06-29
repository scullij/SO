/*
 * protocol.h
 *
 *  Created on: 29/04/2013
 *      Author: utnso
 */

#ifndef PROTOCOL_H_
#define PROTOCOL_H_

#include <stdint.h>

int16_t recibir(int16_t socket, void** payload);

int16_t enviar(int16_t socket, int16_t tipoEnvio, void* payload, uint16_t payloadLenght);

typedef struct {
	uint16_t type;
	uint16_t length;
} __attribute__ ((__packed__)) header_t;

#endif /* PROTOCOL_H_ */
