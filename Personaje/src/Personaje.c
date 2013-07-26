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
#include <commons/config.h>

#define LOCALHOST "127.0.0.1"
#define DIRECCION "127.0.0.1"
#define PUERTO_ORQ 30000
#define PUERTO_PLANIFICADOR 30000
#define PUERTO_NIVEL 30005

typedef struct {
	int16_t x;
	int16_t y;
} __attribute__ ((__packed__)) t_posicion;

typedef struct {
	char* nombre;
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
} t_personaje;

t_personaje *configurar_personaje(char *path);

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
		int16_t puerto;
		char direccion[16];
	} __attribute__ ((__packed__)) t_direccionPuerto;

	typedef struct {
		int16_t nivel;
		int16_t puerto;
		char direccion[16];
	} __attribute__ ((__packed__)) t_nivelDireccionPuerto;

	typedef struct{
		char personaje;
		int socket;
		char recurso;
	} __attribute__ ((__packed__)) t_nodoPerPLa;

	t_personaje* personaje = configurar_personaje(path);

    void *buffer;    // buffer for client data
	int16_t type;

	uint16_t orquestador = create_and_connect(personaje->orquestador, personaje->puertoOrq);

	if (orquestador < 0){
		perror("Error al conectarse al orquestador.");
		return EXIT_FAILURE;
	}

	//ME CONECTO AL ORQUESTADOR
	int* nivelActual = malloc(sizeof(int));
	*nivelActual = personaje->niveles[personaje->indiceNivelActual];
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
	uint16_t planificador = create_and_connect(personaje->orquestador, puertoPlanificador);
	if (planificador < 0){
		perror("Error al conectarse al planificador.");
		return EXIT_FAILURE;
	}

	log_trace(logger, "Conectado al Planificador del Nivel %d, Puerto: %d.", *nivelActual, puertoPlanificador);

	enviar(planificador, P_PER_CONECT_PLANI, &personaje->simbolo, sizeof(char));
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
	char* recurso = malloc(sizeof(char));
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

		if(personaje->objetivo->x == NULL){
			*recurso = personaje->recursos[*nivelActual][personaje->recursoActual];
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
			personaje->objetivo->x = posicion->x;
			personaje->objetivo->y = posicion->y;
		}else{

			if(personaje->posicion->x == personaje->objetivo->x && personaje->posicion->y == personaje->objetivo->y){
				enviar(nivel, P_PER_PEDIR_RECURSO, NULL, 0);
				*quantum = 0;
				type = recibir(nivel, NULL);
				if(type == P_NIV_RECURSO_OK){//RECURSO ASIGNADO
					personaje->objetivo->x = NULL;
					personaje->objetivo->y = NULL;
					personaje->recursoActual++;
					log_trace(logger, "Recurso asignado.");
				}else{
					log_trace(logger, "Personaje bloqueado");
					t_nodoPerPLa* nodo = malloc(sizeof(t_nodoPerPLa));
					nodo->personaje = personaje->simbolo;
					nodo->recurso = *recurso;
					enviar(planificador, P_PER_BLOQ_RECURSO, nodo, sizeof(nodo));
					continue;
				}
			}else{
				if(personaje->posicion->x != personaje->objetivo->x){
					if(personaje->posicion->x > personaje->objetivo->x){
						personaje->posicion->x--;
					}else{
						personaje->posicion->x++;
					}
				}else if(personaje->posicion->y != personaje->objetivo->y){
					if(personaje->posicion->y > personaje->objetivo->y){
						personaje->posicion->y--;
					}else{
						personaje->posicion->y++;
					}
				}
				sleep(1);
				enviar(nivel, P_PER_MOV, personaje->posicion, sizeof(personaje->posicion));
				type = recibir(nivel, NULL);
				*quantum = *quantum - 1;
				log_trace(logger, "Nueva posicion: X:%d Y:%d \n", personaje->posicion->x, personaje->posicion->y);
			}
		}

		if(*quantum == 0){
			if(personaje->recursos[*nivelActual][personaje->recursoActual] == '0'){
				//LE AVISO AL NIVEL Y AL PLANIFICADOR QUE TERMINE
				enviar(nivel, P_PER_NIV_FIN, NULL, 0);
				enviar(planificador, P_PER_NIV_FIN, NULL, 0);
				close(nivel);
				close(planificador);
				log_trace(logger, "Nivel terminado.");
				personaje->indiceNivelActual++;
				*nivelActual = personaje->niveles[personaje->indiceNivelActual];
			}else if(1){ //Esta bloqueado

			}
			log_trace(logger, "Turno terminado.");
			enviar(planificador, P_PER_TURNO_FINALIZADO, NULL, 0);
		}
		fflush(stdout);
	}
	free(quantum);
	close(nivel);
	close(orquestador);
	free(personaje);

	return EXIT_SUCCESS;
}

t_personaje *configurar_personaje(char *path){
	t_personaje* personaje = malloc(sizeof(t_personaje));
	t_config *config = config_create(path);

	char *orquestador = config_get_string_value(config, "Orquestador");
	char** direccionPuerto = string_split(orquestador, ":");
	personaje->orquestador = string_duplicate(direccionPuerto[0]);
	personaje->puertoOrq = atoi(direccionPuerto[1]);
	free(direccionPuerto);
	free(orquestador);

	personaje->nombre = config_get_string_value(config, "Nombre");
	personaje->simbolo = config_get_string_value(config, "Simbolo")[0];

	char** niveles = config_get_array_value(config, "PlanDeNiveles");
	int cantNiveles = 0;
    int i = 0;
    while (niveles[i] != NULL) {
		personaje->niveles[i] = atoi(niveles[i]);
    	i++;
    }
    cantNiveles = i;

    int var;
	for (var = 0; var < cantNiveles; ++var) {
		char** recursos = config_get_array_value(config, string_from_format("Objetivo%d", var+1));
		i=0;
		while(recursos[i] != NULL){
			personaje->recursos[personaje->niveles[var]][i] = *(char*)recursos[i];
			i++;
		}
	}

	personaje->vidas = config_get_int_value(config, "Vidas");

	personaje->posicion = malloc(sizeof(t_posicion));
	personaje->posicion->x = 1;
	personaje->posicion->y = 1;

	personaje->objetivo = malloc(sizeof(t_posicion));
	personaje->objetivo->x = NULL;

	personaje->recursoActual = 0;
	personaje->indiceNivelActual = 0;

	config_destroy(config);
	return personaje;
}
