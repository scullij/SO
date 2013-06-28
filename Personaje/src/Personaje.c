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

#include <commons/string.h>

#define DIRECCION "127.0.0.1"
#define PUERTO 30000

int main() {

	typedef struct {
		char nombre[10];
		char simbolo;
		int niveles[5];
		char recursos[5][10];
		int vidas;
		char* orquestador;
	} personaje;

	personaje p;
	strcpy(p.nombre, "Mario");
	//strcpy(p.simbolo,"@");
	p.niveles[0]=1;
	p.niveles[1]=2;
	//strcpy(p.recursos[1][0],"F");
	//strcpy(p.recursos[1][1],"H");
	//strcpy(p.recursos[2][0],"M");
	p.vidas=5;
	p.orquestador = malloc(strlen(DIRECCION)+1);
	strcpy(p.orquestador, DIRECCION);

	puts("Personaje...");

	uint16_t plataforma = create_socket();

	if (connect_socket(plataforma, p.orquestador, PUERTO) < 0){
		perror("Error al conectarse.");
		return EXIT_FAILURE;
	}

	printf("Conectado!\n");

	while (1) {
		char *buffer = malloc(0);
		strcpy(buffer, "PEDIDO-NIVEL");

		// Enviar los datos leidos por teclado a traves del socket.
		if (enviar(plataforma, buffer, 1) >= 0) {
			printf("Datos enviados!\n");
		}

		buffer = string_new();
		recibir(plataforma, &buffer);

		free(buffer);
	}

	close(plataforma);

	return EXIT_SUCCESS;

}
