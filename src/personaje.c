/*
 * personaje.c
 *
 *  Created on: 28/04/2013
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

#include "socket.h"

#define DIRECCION "127.0.0.1"
#define PUERTO 10000
#define BUFF_SIZE 1024

int personaje() {

	int socket = create_socket();

	if (connect_socket(socket, DIRECCION, 10000) < 0){
		perror("Error al conectarse.");
		return EXIT_FAILURE;
	}

	char buffer[BUFF_SIZE];

	printf("Conectado!\n");

	while (1) {

		scanf("%s", buffer);

		// Enviar los datos leidos por teclado a traves del socket.
		if (send(socket, buffer, strlen(buffer), 0) >= 0) {
			printf("Datos enviados!\n");

			if (strcmp(buffer, "fin") == 0) {

				printf("Cliente cerrado correctamente.\n");
				break;

			}

		} else {
			perror("Error al enviar datos. Server no encontrado.\n");
			break;
		}
	}

	close(socket);

	return EXIT_SUCCESS;

}
