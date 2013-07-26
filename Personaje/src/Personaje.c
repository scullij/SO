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
#include <commons/log.h>

#define LOCALHOST "127.0.0.1"
#define DIRECCION "127.0.0.1"
#define PUERTO_ORQ 30000
#define PUERTO_PLANIFICADOR 30000
#define PUERTO_NIVEL 30005

int main(int argc, char *argv[]){
	t_log* logger;
	logger = log_create("personaje.log", "personaje", true, LOG_LEVEL_TRACE);
	char* path = malloc(0);
	if(argc < 2){
		puts("Ingrese el path de configuracion:");
		scanf("%s", path);
	}else {
		path = string_duplicate(argv[1]);
	}

	typedef struct {
		int16_t x;
		int16_t y;
	} __attribute__ ((__packed__)) t_posicion;

	typedef struct {
		int16_t puerto;
		char direccion[16];
	} __attribute__ ((__packed__)) t_direccionPuerto;

	typedef struct {
		int16_t nivel;
		int16_t puerto;
		char direccion[16];
	} __attribute__ ((__packed__)) t_nivelDireccionPuerto;

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

		int indiceNivelActual;
	} personaje;

	personaje p;
	strcpy(p.nombre, "Mario");
	//strcpy(p.simbolo,"@");
	p.niveles[0]=1;
	p.niveles[1]=2;
	p.recursos[1][0] = 'F';
	p.recursos[1][1] = 'H';
	p.recursos[1][2] = 'M';
	p.recursos[1][3] = '0';
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
	p.indiceNivelActual = 0;

    void *buffer;    // buffer for client data
	int16_t type;

	uint16_t orquestador = create_and_connect(p.orquestador, p.puertoOrq);

	if (orquestador < 0){
		perror("Error al conectarse al orquestador.");
		return EXIT_FAILURE;
	}

	//ME CONECTO AL ORQUESTADOR
	int* nivelActual = malloc(sizeof(int));
	*nivelActual = p.niveles[p.indiceNivelActual];
	enviar(orquestador, P_PER_CONECT_PLA, nivelActual, sizeof(int));

	log_trace(logger, "Personaje conectado al orquestador.");

	//DATOS NIVEL
	type = recibir(orquestador, &buffer);
	t_nivelDireccionPuerto* nivelDirecionPuerto = malloc(sizeof(t_nivelDireccionPuerto));
	memcpy(nivelDirecionPuerto, buffer, sizeof(t_nivelDireccionPuerto));


	//DATOS PLANIFICADOR
	type = recibir(orquestador, &buffer);
	int puertoPlanificador = (*(int*)buffer);


	//ME DESCONECTO DEL ORQUESTADOR
	close(orquestador);

	//ME CONECTO AL PLANIFICADOR
	uint16_t planificador = create_and_connect(p.orquestador, puertoPlanificador);
	if (planificador < 0){
		perror("Error al conectarse al planificador.");
		return EXIT_FAILURE;
	}

	log_trace(logger, "Conectado al Planificador del Nivel %d, Puerto: %d.", *nivelActual, puertoPlanificador);

	enviar(planificador, P_PER_CONECT_PLANI, NULL, 0);
	type = recibir(planificador, &buffer);
	if(type != P_PLA_ACEPT_PER){
		perror("Planificador rechaza personaje.");
		return EXIT_FAILURE;
	}

	//ME CONECTO AL NIVEL
	uint16_t nivel = create_and_connect(nivelDirecionPuerto->direccion, nivelDirecionPuerto->puerto);
	if (nivel < 0){
		perror("Error al conectarse al nivel.");
		return EXIT_FAILURE;
	}

	log_trace(logger, "Conectado al Nivel %d: Direccion: %s, Puerto: %d.", *nivelActual, nivelDirecionPuerto->direccion, nivelDirecionPuerto->puerto);

	//HANDSHAKE NIVEL
	enviar(nivel, P_PER_CONECT_NIV, NULL, 0);
	type = recibir(nivel, NULL);

	if(type != P_NIV_ACEPT_PER){
		perror("Nivel rechaza personaje.\n");
		return EXIT_FAILURE;
	}

	int* quantum = malloc(sizeof(int));
	*quantum = 0;
	while (1)
	{
		if(*quantum == 0){
			type = recibir(planificador, &quantum);
			if(type != P_PLA_MOV_PERMITIDO){
				continue;
			}
			log_trace(logger, "Quantum Recibido %d\n", *quantum);
			fflush(stdout);
		}

		if(p.objetivo->x == NULL){
			char* recurso = malloc(sizeof(char));
			*recurso = p.recursos[*nivelActual][p.recursoActual];
			enviar(nivel, P_PER_LUGAR_RECURSO, recurso, sizeof(char));
			free(recurso);

			t_posicion* posicion = malloc(sizeof(t_posicion));
			type = recibir(nivel, &posicion);

			if(type != P_NIV_UBIC_RECURSO){
				continue;
			}

			if(posicion->x < 0){
				log_trace(logger, "Recurso inexistente en el nivel, imposible temrinar");
				close(nivel);
				close(planificador);
				//TODO: LIBERAR RECURSOS DLE NIVEL TOMADOS
				exit(EXIT_FAILURE);
			}

			log_trace(logger, "Siguiente posicion de objetivo : x: %d y: %d\n", posicion->x, posicion->y);
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
					log_trace(logger, "Recurso asignado.");
				}else{
					log_trace(logger, "Personaje bloqueado");
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
				log_trace(logger, "Nueva posicion: X:%d Y:%d \n", p.posicion->x, p.posicion->y);
			}
		}

		if(*quantum == 0){
			if(p.recursos[*nivelActual][p.recursoActual] == '0'){
				//LE AVISO AL NIVEL Y AL PLANIFICADOR QUE TERMINE
				enviar(nivel, P_PER_NIV_FIN, NULL, 0);
				enviar(planificador, P_PER_NIV_FIN, NULL, 0);
				close(nivel);
				close(planificador);
				log_trace(logger, "Nivel terminado.");
				p.indiceNivelActual++;
				*nivelActual = p.niveles[p.indiceNivelActual];
			}
			log_trace(logger, "Turno terminado.");
			enviar(planificador, P_PER_TURNO_FINALIZADO, NULL, 0);
		}
		fflush(stdout);
	}

	free(quantum);
	close(nivel);
	close(orquestador);

	return EXIT_SUCCESS;
}
