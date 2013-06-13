/*
 * plataforma.c
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

#define DIRECCION INADDR_ANY   //INADDR_ANY representa la direccion de cualquier
							   //interfaz conectada con la computadora
#define PUERTO 30000
#define BUFF_SIZE 1024

int main() {

	puts("Plataforma...");

	uint16_t socket = create_and_listen(PUERTO);
	if(socket == -1){
		return EXIT_FAILURE;
	}

	uint16_t accepted_connection = accept_connection(socket);

	uint16_t lenght;

	while (1) {

		void* buffer = malloc(0);

		lenght = recv_variable(accept_connection, buffer);

		if (lenght > 0) {
			printf("Mensaje recibido: ");
			fwrite(buffer, 1, lenght, stdout);
			printf("\n");
			printf("Tamanio del buffer %d bytes!\n", lenght);
			fflush(stdout);

			if (memcmp(buffer, "fin", lenght) == 0) {

				printf("Server cerrado correctamente.\n");
				break;

			}

		}
	}

	close(socket);
	close(accepted_connection);

	return EXIT_SUCCESS;
}
