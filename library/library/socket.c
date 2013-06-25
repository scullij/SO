/*
 * socket.c
 *
 *  Created on: 29/04/2013
 *      Author: utnso
 */

#include <stdlib.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>

#define DIRECCION INADDR_ANY   //INADDR_ANY representa la direccion de cualquier
							   //interfaz conectada con la computadora
#define BUFF_SIZE 1024

#define CONNECTIONS 10

uint16_t create_socket(){
	uint16_t sockete;

	// Crear un socket:
	// AF_INET: Socket de internet IPv4
	// SOCK_STREAM: Orientado a la conexion, TCP
	// 0: Usar protocolo por defecto para AF_INET-SOCK_STREAM: Protocolo TCP/IPv4
	if ((sockete = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("Error al crear socket");
		return EXIT_FAILURE;
	}

	return sockete;
}

uint16_t create_and_listen(uint16_t puerto){

	uint16_t socket = create_socket();

	struct sockaddr_in socketInfo;
	uint16_t optval = 1;


	// Hacer que el SO libere el puerto inmediatamente luego de cerrar el socket.
	setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &optval,
			sizeof(optval));

	socketInfo.sin_family = AF_INET;
	socketInfo.sin_addr.s_addr = DIRECCION; //Notar que aca no se usa inet_addr()
	socketInfo.sin_port = htons(puerto);

	// Vincular el socket con una direccion de red almacenada en 'socketInfo'.
	if (bind(socket, (struct sockaddr*) &socketInfo, sizeof(socketInfo))
			!= 0) {
		perror("Error al bindear socket escucha");
		return -1;
	}

	if (listen(socket, CONNECTIONS) != 0) {
		perror("Error al poner a escuchar socket");
		return -1;
	}else {
		printf("Socket Plataforma escuchando en el puerto: %u \n", &puerto);
	}

	return socket;
}

uint16_t connect_socket(uint16_t socket, __const char* direccion, uint16_t puerto){
	struct sockaddr_in socketInfo;

	socketInfo.sin_family = AF_INET;
	socketInfo.sin_addr.s_addr = inet_addr(direccion);
	socketInfo.sin_port = htons(puerto);

	// Conectar el socket con la direccion 'socketInfo'.
	if (connect(socket, (struct sockaddr*) &socketInfo, sizeof(socketInfo))
			!= 0) {
		perror("Error al conectar socket");
		return -1;
	}

	return EXIT_SUCCESS;
}

uint16_t create_and_connect(uint16_t socket, char* direccion, uint16_t puerto){
	uint16_t sockete = create_socket();
	return connect_socket(sockete, direccion, puerto);
}

uint16_t accept_connection(uint16_t socket){

	uint16_t socketNuevaConexion;

	// Aceptar una nueva conexion entrante. Se genera un nuevo socket con la nueva conexion.
	if ((socketNuevaConexion = accept(socket, NULL, 0)) < 0) {

		perror("Error al aceptar conexion entrante");
		return -1;

	}

	return socketNuevaConexion;
}
