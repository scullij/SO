/*
 * socket.h
 *
 *  Created on: 29/04/2013
 *      Author: utnso
 */

#ifndef SOCKET_H_
#define SOCKET_H_

int8_t create_socket();

int8_t create_and_listen(int8_t puerto);

int8_t create_and_connect(int8_t puerto);

int8_t connect_socket(int8_t socket, char* direccion, int8_t puerto);

int8_t accept_connection(int8_t socket);

#endif /* SOCKET_H_ */
