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

#define PUERTO 30000
#define BUFF_SIZE 1024

int main() {

	puts("Plataforma...");

	uint16_t socket = create_and_listen(PUERTO);
	if(socket == -1){
		return EXIT_FAILURE;
	}

	uint16_t accepted_connection = accept_connection(socket);

	while (1) {

		char* buffer = malloc(0);

		int16_t type = recibir(accepted_connection, &buffer);

		printf("Mensaje recibido: %d - %s \n", type, buffer);
		fflush(stdout);

		if (memcmp(buffer, "fin", 3) == 0) {
			puts("Server cerrado correctamente");
			break;
		}

		free(buffer);
	}


	close(socket);
	close(accepted_connection);

	return EXIT_SUCCESS;
}
