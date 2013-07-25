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

#include <library/messages.h>
#include <library/socket.h>
#include <library/protocol.h>

#include <commons/string.h>

#define LOCALHOST "127.0.0.1"
#define DIRECCION "127.0.0.1"
#define PUERTO_ORQ 30000
#define PUERTO_PLANIFICADOR 30000
#define PUERTO_NIVEL 30005

int main() {

	typedef struct {
		int16_t x;
		int16_t y;
	} __attribute__ ((__packed__)) t_posicion;

	typedef struct {
		char nombre[10];
		char simbolo;
		int niveles[5];
		char recursos[5][10];
		int vidas;
		char* orquestador;
		uint16_t puertoOrq;

		t_posicion* objetivo;
		t_posicion* posicion;
		int recursoActual;
	} personaje;

	personaje p;
	strcpy(p.nombre, "Mario");
	//strcpy(p.simbolo,"@");
	p.niveles[0]=1;
	p.niveles[1]=2;
	p.recursos[0][0] = 'F';
	p.recursos[0][1] = 'H';
	p.recursos[0][2] = 'M';
	p.vidas=5;
	p.orquestador = malloc(strlen(LOCALHOST)+1);
	strcpy(p.orquestador, LOCALHOST);
	p.puertoOrq = PUERTO_ORQ;

	p.posicion = malloc(sizeof(t_posicion));
	p.posicion->x = 1;
	p.posicion->y = 1;

	p.objetivo = malloc(sizeof(t_posicion));
	p.objetivo->x = NULL;

	p.recursoActual = 0;

	puts("Personaje...");

	uint16_t planificador = create_and_connect(p.orquestador, p.puertoOrq);

	if (planificador < 0){
		perror("Error al conectarse al planificador.");
		return EXIT_FAILURE;
	}

	printf("Conectado!\n");

	enviar(planificador, P_PER_CONECT_PLA, NULL, 0);

	int16_t type = recibir(planificador, NULL);

	if(type != P_PLA_ACEPT_PER){
		perror("Planificador rechaza personaje.");
		return EXIT_FAILURE;
	}

	printf("Personaje Aceptado por planificador.\n");

	uint16_t nivel = create_socket();
	if (connect_socket(nivel, LOCALHOST, PUERTO_NIVEL) < 0){
		perror("Error al conectarse al nivel.");
		return EXIT_FAILURE;
	}

	enviar(nivel, P_PER_CONECT_NIV, NULL, 0);

	type = recibir(nivel, NULL);

	if(type != P_NIV_ACEPT_PER){
		perror("Nivel rechaza personaje.\n");
		return EXIT_FAILURE;
	}

	printf("Personaje Aceptado por nivel.\n");

	int* quantum = malloc(sizeof(int));
	*quantum = 0;
	while (1)
	{
		if(*quantum == 0){
			type = recibir(planificador, &quantum);
			if(type != P_PLA_MOV_PERMITIDO){
				continue;
			}
			printf("Quantum Recibido %d\n", *quantum);
			fflush(stdout);
		}

		if(p.objetivo->x == NULL){
			char* recurso = malloc(sizeof(char));
			*recurso = p.recursos[0][p.recursoActual];
			enviar(nivel, P_PER_LUGAR_RECURSO, recurso, sizeof(char));
			free(recurso);

			t_posicion* posicion = malloc(sizeof(t_posicion));
			type = recibir(nivel, &posicion);

			if(type != P_NIV_UBIC_RECURSO){
				continue;
			}

			printf("Siguiente posicion de objetivo : x: %d y: %d\n", posicion->x, posicion->y);
			fflush(stdout);
			p.objetivo->x = posicion->x;
			p.objetivo->y = posicion->y;
		}else{

			if(p.posicion->x == p.objetivo->x && p.posicion->y == p.objetivo->y){
				enviar(nivel, P_PER_PEDIR_RECURSO, NULL, 0);
				*quantum = 0;
				type = recibir(nivel, NULL);
				if(type == P_NIV_RECURSO_OK){//RECURSO ASIGNADO
					p.objetivo->x = NULL;
					p.objetivo->y = NULL;
					p.recursoActual++;
					puts("Recurso asignado.");
				}else{
					puts("Personaje bloqueado");
					enviar(planificador, P_PER_BLOQ_RECURSO, NULL, 0);
				}
			}else{
				if(p.posicion->x != p.objetivo->x){
					if(p.posicion->x > p.objetivo->x){
						p.posicion->x--;
					}else{
						p.posicion->x++;
					}
				}else if(p.posicion->y != p.objetivo->y){
					if(p.posicion->y > p.objetivo->y){
						p.posicion->y--;
					}else{
						p.posicion->y++;
					}
				}
				sleep(1);
				enviar(nivel, P_PER_MOV, p.posicion, sizeof(p.posicion));
				type = recibir(nivel, NULL);
				*quantum = *quantum - 1;
				printf("Nueva posicion: X:%d Y:%d \n", p.posicion->x, p.posicion->y);
			}
		}

		if(*quantum == 0){
			enviar(planificador, P_PER_TURNO_FINALIZADO, NULL, 0);
			puts("Turno terminado.");
		}
		fflush(stdout);
	}

	free(quantum);
	close(nivel);
	close(planificador);

	return EXIT_SUCCESS;
}
