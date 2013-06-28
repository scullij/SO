/*
 ============================================================================
 Name        : Nivel.c
 Author      : 
 Version     :
 Copyright   : 
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <commons/collections/list.h>
#include <library/socket.h>
#include <library/messages.h>
#include <library/protocol.h>
#include <commons/string.h>

#define DIRECCION "127.0.0.1"
#define PUERTO 30000

int main(void) {

	typedef struct {
		char* nombre;
		char simbolo;
		int instancias;
		int x;
		int y;
	} recurso;

	typedef struct {
		//TODO Nombre es string
		int nombre;
		recurso recursos[3];
		char* orquestador;
		int deadlock;
		char recovery;
	} nivel;

	nivel niv;
	niv.nombre = 1;
	niv.orquestador = malloc(strlen(DIRECCION));
	strcpy(niv.orquestador, DIRECCION);

	puts("Nivel");

	uint16_t plataforma = create_socket();

	if (connect_socket(plataforma, niv.orquestador, PUERTO) < 0){
		perror("nivel-coneccion-plataforma");
		return EXIT_FAILURE;
	}

	printf("Conectado!\n");

	if(enviar(plataforma, niv.nombre, 30)==-1){
		perror("nivel-enviar-plataforma");
		return EXIT_FAILURE;
	}

	char* buffer = string_new();
	int16_t type = recibir(plataforma, buffer);


	char** direccionPuerto = string_split(buffer, ":");
	uint16_t planificador = create_socket();
	if (connect_socket(planificador, direccionPuerto[0], direccionPuerto[1]) < 0){
		perror("nivel-coneccion-planificador");
		return EXIT_FAILURE;
	}
	free(direccionPuerto);

	enviar(planificador, "READY", 31);

	free(buffer);
	close(plataforma);
	close(planificador);

	return EXIT_SUCCESS;
}
