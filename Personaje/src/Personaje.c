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

#include <library/socket.h>
#include <library/protocol.h>

#define DIRECCION "127.0.0.1"
#define PUERTO 30000

int main() {

	puts("Personaje...");

	uint16_t socket = create_socket();

	if (connect_socket(socket, DIRECCION, PUERTO) < 0){
		perror("Error al conectarse.");
		return EXIT_FAILURE;
	}

	printf("Conectado!\n");

	while (1) {
		char *buffer = malloc(0);
		scanf("%s", buffer);

		// Enviar los datos leidos por teclado a traves del socket.
		if (enviar(socket, buffer, 1) >= 0) {
			printf("Datos enviados!\n");

			if (strcmp(buffer, "fin") == 0) {
				printf("Cliente cerrado correctamente.\n");
				break;
			}
		} else {
			perror("Error al enviar datos. Server no encontrado.\n");
			break;
		}
		free(buffer);
	}

	close(socket);

	return EXIT_SUCCESS;

}
