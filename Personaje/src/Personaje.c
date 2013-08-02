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

typedef struct {
	int16_t x;
	int16_t y;
	char personaje;
} __attribute__ ((__packed__)) t_posicion;

typedef struct {
	char* nombre;
	char simbolo;
	int niveles[10];
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

	char* personajeLog = string_from_format("personaje%c", personaje->simbolo);
	t_log* logger;
	logger = log_create(personajeLog, personajeLog, true, LOG_LEVEL_TRACE);

	void *buffer;    // buffer for client data
	int16_t type;

	uint16_t orquestador;

	int* nivelActual = malloc(sizeof(int));
	while(personaje->niveles[personaje->indiceNivelActual] != 0){
		orquestador = create_socket();
		connect_socket(orquestador, personaje->orquestador, personaje->puertoOrq);

		if (orquestador < 0){
			perror("Error al conectarse al orquestador.");
			return EXIT_FAILURE;
		}

		//ME CONECTO AL ORQUESTADOR
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
		enviar(nivel, P_PER_CONECT_NIV, &personaje->simbolo, sizeof(char));
		type = recibir(nivel, &buffer);

		if(type != P_NIV_ACEPT_PER){
			perror("Nivel rechaza personaje.");
			return EXIT_FAILURE;
		}

		int* quantum = malloc(sizeof(int));
		*quantum = 0;
		char* recurso = malloc(sizeof(char));
		while (1)
		{
			if(*quantum == 0){
				type = recibir(planificador, &quantum);
				if(type == 64){ //Te mataron papu
					close(nivel);
					close(planificador);
					log_trace(logger, "Me mataron.");
					personaje->recursoActual=0;
					personaje->posicion->x = 1;
					personaje->posicion->y = 1;
					personaje->objetivo->x = 0;
					personaje->objetivo->y = 0;
					personaje->vidas--;
					if(personaje->vidas <= 0){
						personaje->indiceNivelActual = 0;
					}
					break;
				}else if(type == P_PLA_MOV_PERMITIDO){
					log_trace(logger, "Quantum Recibido %d", *quantum);
					fflush(stdout);
				}else{
					continue;
				}
			}

			if(personaje->objetivo->x == 0){
				*recurso = personaje->recursos[*nivelActual][personaje->recursoActual];
				enviar(nivel, P_PER_LUGAR_RECURSO, recurso, sizeof(char));

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

				log_trace(logger, "Siguiente posicion de objetivo : x: %d y: %d", posicion->x, posicion->y);
				fflush(stdout);
				personaje->objetivo->x = posicion->x;
				personaje->objetivo->y = posicion->y;
			}else{

				if(personaje->posicion->x == personaje->objetivo->x && personaje->posicion->y == personaje->objetivo->y){
					enviar(nivel, P_PER_PEDIR_RECURSO, personaje->posicion, sizeof(t_posicion));
					*quantum = 0;
					type = recibir(nivel, NULL);
					if(type == P_NIV_RECURSO_OK){//RECURSO ASIGNADO
						personaje->objetivo->x = 0;
						personaje->objetivo->y = 0;
						log_trace(logger, "Recurso asignado %c.", personaje->recursos[*nivelActual][personaje->recursoActual]);
						personaje->recursoActual++;
					}else{
						log_trace(logger, "Personaje bloqueado por recurso %c.", personaje->recursos[*nivelActual][personaje->recursoActual]);
						enviar(planificador, P_PER_BLOQ_RECURSO, recurso, sizeof(char));
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
					usleep(100000);
					enviar(nivel, P_PER_MOV, personaje->posicion, sizeof(t_posicion));
					type = recibir(nivel, NULL);
					*quantum = *quantum - 1;
					log_trace(logger, "Nueva posicion: X:%d Y:%d", personaje->posicion->x, personaje->posicion->y);
				}
			}

			if(*quantum == 0){
				if(personaje->recursos[*nivelActual][personaje->recursoActual] == '\0'){
					//LE AVISO AL NIVEL Y AL PLANIFICADOR QUE TERMINE
					log_trace(logger, "Estoy para terminar.");
					enviar(nivel, P_PER_NIV_FIN, &personaje->simbolo, sizeof(char));
					enviar(planificador, P_PER_NIV_FIN, &personaje->simbolo, sizeof(char));
					recibir(planificador, NULL);
					close(nivel);
					close(planificador);
					log_trace(logger, "Nivel terminado.");
					personaje->indiceNivelActual++;
					personaje->recursoActual=0;
					personaje->posicion->x = 1;
					personaje->posicion->y = 1;
					*nivelActual = personaje->niveles[personaje->indiceNivelActual];
					break;
				}
				log_trace(logger, "Turno terminado.");
				enviar(planificador, P_PER_TURNO_FINALIZADO, NULL, 0);
			}
			fflush(stdout);
		}
		free(recurso);
		free(quantum);

	}

	log_trace(logger, "Personaje %c finalizo su plan de niveles", personaje->simbolo);
	orquestador = create_socket();
	connect_socket(orquestador, personaje->orquestador, personaje->puertoOrq);
	enviar(orquestador, 159, NULL, 0);

	free(logger);
	free(personaje);
	close(orquestador);

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
    personaje->niveles[i] = 0;
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
	personaje->posicion->personaje = personaje->simbolo;

	personaje->objetivo = malloc(sizeof(t_posicion));
	personaje->objetivo->x = 0;

	personaje->recursoActual = 0;
	personaje->indiceNivelActual = 0;

	//config_destroy(config);
	return personaje;
}
