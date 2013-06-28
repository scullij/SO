/*
 * socket.h
 *
 *  Created on: 29/04/2013
 *      Author: utnso
 */

#ifndef SOCKET_H_
#define SOCKET_H_

int16_t create_socket();

uint16_t create_and_listen(int16_t puerto);

int16_t create_and_connect(int16_t puerto);

int16_t connect_socket(int16_t socket, char* direccion, int16_t puerto);

int16_t accept_connection(int16_t socket);

#endif /* SOCKET_H_ */
