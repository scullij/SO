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
#include <netdb.h>
#include <inttypes.h>
#include <pthread.h>

#include <library/socket.h>
#include <library/protocol.h>
#include <library/messages.h>

#include <commons/collections/list.h>
#include <commons/collections/dictionary.h>
#include <commons/collections/queue.h>
#include <commons/config.h>
#include <commons/log.h>

t_log* logger;

typedef struct {
	t_queue* rr;
	t_queue* bloqueados;
	char personajeActivo;
} t_planificacionNodo;

t_dictionary* planificacion;

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

typedef struct {
	char *direccion;
	uint16_t puerto;
	int deteccionInterbloqueo;
	int tiempoDeteccionInterbloqueo;
	int quantum;
	int retardo;
} t_plataforma;

typedef struct {
	int nivel;
	int puerto;
	t_planificacionNodo* planificacionNodo;
} t_plaThread;

t_plataforma *plataforma;

t_plataforma *configurar_plataforma(char* path);

void rutinasOrquestador(int sockete, int routine, void* payload);
void rutinasPlanificador(int sockete, int routine, void* payload, t_planificacionNodo* planificacion, int nivel);
int orquestador(void);
int planificador(void* ptr);

int main(int argc, char *argv[]){
	logger = log_create("plataforma.log", "plataforma", "true", LOG_LEVEL_TRACE);
	char* path = malloc(0);
	if(argc < 2){
		puts("Ingrese el path de configuracion:");
		scanf("%s", path);
	}else {
		path = string_duplicate(argv[1]);
	}

	plataforma = configurar_plataforma(path);
	free(path);

	planificacion = dictionary_create();

	pthread_t thr_orquestador;

	pthread_create( &thr_orquestador, NULL, orquestador, NULL);

	pthread_join( thr_orquestador, NULL);

	free(plataforma);
	free(logger);
	return EXIT_SUCCESS;
}

int orquestador(void){
	t_dictionary* niveles = dictionary_create();
	t_dictionary* planificadorPuerto = dictionary_create();
	int puertoInicialPlanificador = 30020;

    fd_set master;    // master file descriptor list
    fd_set read_fds;  // temp file descriptor list for select()
    uint16_t fdmax;        // maximum file descriptor number

    uint16_t listener;     // listening socket descriptor
    unsigned int newfd;        // newly accept()ed socket descriptor

    void *buffer;    // buffer for client data
    int type;

	listener = create_and_listen(plataforma->puerto);
	if(listener == -1){
		perror("listener");
		return EXIT_FAILURE;
	}

	log_trace(logger, "Orquestador: Listen %s, Port %u", plataforma->direccion, plataforma->puerto);

    FD_ZERO(&master);    // clear the master and temp sets
    FD_ZERO(&read_fds);

    // add the listener to the master set
    FD_SET(listener, &master);

    // keep track of the biggest file descriptor
    fdmax = listener; // so far, it's this one

    // main loop
    for(;;) {
        read_fds = master; // copy it
        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(1);
        }

        int i;
        // run through the existing connections looking for data to read
        for(i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) { // we got one!!
                if (i == listener) {
                    // handle new connections
                	char direccion[16];
                    newfd = accept_connection(listener, direccion);

                    if (newfd == -1) {
                        perror("accept");
                    } else {
                        //HANDSHAKE
						type = recibir(newfd, &buffer);
                        if(type == P_NIV_CONNECT_ORQ){ //NIVEL

                        	//SOLO LO AGREGO AL NIVEL, EL PERSONAJE SE VA A DESCONECTAR
                        	FD_SET(newfd, &master); // add to master set
							if (newfd > fdmax) {    // keep track of the max
								fdmax = newfd;
							}

                        	t_nivelDireccionPuerto* nivelDirecionPuerto = malloc(sizeof(t_nivelDireccionPuerto));
                        	memcpy(nivelDirecionPuerto, buffer, sizeof(t_nivelDireccionPuerto));
							strcpy(nivelDirecionPuerto->direccion, direccion);

                        	log_trace(logger, "Orquestador: Nuevo Nivel %d conectado: Direccion: %s Puerto: %d.\n", nivelDirecionPuerto->nivel, nivelDirecionPuerto->direccion, nivelDirecionPuerto->puerto);

                        	char* key = malloc(7);
                        	strcpy(key, string_from_format("nivel%d", nivelDirecionPuerto->nivel));

                        	t_planificacionNodo* planificacionNodo = malloc(sizeof(t_planificacionNodo));
                        	planificacionNodo->rr = queue_create();
                        	planificacionNodo->bloqueados = queue_create();
                        	planificacionNodo->personajeActivo = 0;
                        	dictionary_put(planificacion, key, planificacionNodo);

                        	pthread_t thr_pla;
							t_plaThread pla;
							pla.nivel = nivelDirecionPuerto->nivel;
							pla.puerto = puertoInicialPlanificador;
							pla.planificacionNodo = planificacionNodo;
							pthread_create( &thr_pla, NULL, &planificador, &pla);

							char* newKey = malloc(3);
							sprintf(newKey,"%d", pla.nivel);
							dictionary_put(planificadorPuerto, newKey, pla.puerto);

				            puertoInicialPlanificador++;

                        	dictionary_put(niveles, key, nivelDirecionPuerto);
                        	free(key);
                        }else if(type == P_PER_CONECT_PLA){ //PERSONAJE
                        	log_trace(logger, "Orquestador: Nuevo personaje conectado.\n", newfd);
							char* key = malloc(7);
							strcpy(key, string_from_format("nivel%d", (*(int*)buffer)));
							t_nivelDireccionPuerto* nivelDireccionPuerto;
							nivelDireccionPuerto = dictionary_get(niveles, key);
							free(key);
							enviar(newfd, P_PLA_ENVIO_NIVEL, nivelDireccionPuerto, sizeof(t_nivelDireccionPuerto));
							int* puertoPLanificador = malloc(sizeof(int));

							char* newKey = malloc(3);
							sprintf(newKey,"%d", nivelDireccionPuerto->nivel);
							*puertoPLanificador = dictionary_get(planificadorPuerto, newKey);
							enviar(newfd, P_PLA_ENVIO_PLANI, puertoPLanificador, sizeof(int));
							free(newKey);
							free(nivelDireccionPuerto);
                        }
                    }
                } else {
                    // handle data from a client
                    if ((type = recibir(i,&buffer)) < 0) {
                        // got error or connection closed by client
                    	if(type==-1){
                    		close(i); // bye!
                    		FD_CLR(i, &master); // remove from master set
                    	}else{
                    		perror("Invalid Recv");
                    	}
                    } else {
                        // we got some data from a client
                    	rutinasOrquestador(i, type, buffer);
                    }
                } // END handle data from client
            } // END got new incoming connection
        } // END looping through file descriptors
    } // END for(;;)--and you thought it would never end!


    close(listener);
    free(buffer);
    return EXIT_SUCCESS;
}

int planificador(void* ptr){
	t_plaThread *pla;
	pla = (t_plaThread*)ptr;

	log_trace(logger, "Planificador Nivel %d Started.", pla->nivel);

	fd_set master;    // master file descriptor list
	fd_set read_fds;  // temp file descriptor list for select()
	uint16_t fdmax;        // maximum file descriptor number

	uint16_t listener;     // listening socket descriptor
	unsigned int newfd;        // newly accept()ed socket descriptor

	void *buffer;    // buffer for client data
	int type;

	listener = create_and_listen(pla->puerto);
	if(listener == -1){
		perror("listener planificador");
	}

	log_trace(logger, "Planificador Nivel %d: Listen Port %u", pla->nivel, pla->puerto);

	FD_ZERO(&master);    // clear the master and temp sets
	FD_ZERO(&read_fds);

	// add the listener to the master set
	FD_SET(listener, &master);

	// keep track of the biggest file descriptor
	fdmax = listener; // so far, it's this one

	// main loop
	for(;;) {
		read_fds = master; // copy it
		if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
			perror("select");
			exit(1);
		}

		int i;
		// run through the existing connections looking for data to read
		for(i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &read_fds)) { // we got one!!
				if (i == listener) {
					// handle new connections
					char direccion[16];
					newfd = accept_connection(listener, direccion);

					if (newfd == -1) {
						perror("accept planificador");
					} else {
						FD_SET(newfd, &master); // add to master set
						if (newfd > fdmax) {    // keep track of the max
							fdmax = newfd;
						}
					}
				} else {
					// handle data from a client
					if ((type = recibir(i,&buffer)) < 0) {
						// got error or connection closed by client
						if(type==-1){
							close(i); // bye!
							FD_CLR(i, &master); // remove from master set
						}else{
							perror("Invalid Recv");
						}
					} else {
						// we got some data from a client
						rutinasPlanificador(i, type, buffer, pla->planificacionNodo, pla->nivel);
					}
				} // END handle data from client
			} // END got new incoming connection
		} // END looping through file descriptors
	} // END for(;;)--and you thought it would never end!


	close(listener);

	return EXIT_SUCCESS;
}

void rutinasPlanificador(int sockete, int routine, void* payload, t_planificacionNodo* planificacion, int nivel){
	t_nodoPerPLa* new = malloc(sizeof(t_nodoPerPLa));
	//SOLO ESCUCHA PERSONAJES
	switch (routine) {
		case P_PER_CONECT_PLANI:
            log_trace(logger, "Acepto Personaje: en Socket: %u\n", sockete);
			enviar(sockete, P_PLA_ACEPT_PER, NULL, 0);

			new->personaje = (*(char*)payload);
			new->socket = sockete;

			if(planificacion->personajeActivo == 0){ // ES EL UNICO QUE ESTA POR AHORA
				planificacion->personajeActivo = new->personaje;
				log_trace(logger, "Planificador Nivel %d, Movimiento personaje: %c", nivel, new->personaje);
				enviar(sockete, P_PLA_MOV_PERMITIDO, &plataforma->quantum, sizeof(int));
			}else{
				log_trace(logger, "Planificador Nivel %d, Encolo personaje: %c", nivel, new->personaje);
				queue_push(planificacion->rr, new);
			}
			break;
		case P_PER_TURNO_FINALIZADO:
			planificacion->personajeActivo = 0;

			new->personaje = (*(char*)payload);
			new->socket = sockete;
			queue_push(planificacion->rr, new);

			usleep(100*plataforma->retardo);

			new = (t_nodoPerPLa*)queue_pop(planificacion->rr);
			log_trace(logger, "Planificador Nivel %d, Movimiento personaje: %c", nivel, new->personaje);
			enviar(new->socket, P_PLA_MOV_PERMITIDO, &plataforma->quantum, sizeof(int));
			break;
		case P_PER_BLOQ_RECURSO:
			new->personaje = ((t_nodoPerPLa*)payload)->personaje;
			new->socket = sockete;
			new->recurso = ((t_nodoPerPLa*)payload)->recurso;
			queue_push(planificacion->bloqueados, new);
			log_trace(logger, "Planificador Nivel %d, Bloqueo personaje: %c", nivel, new->personaje);
			break;
		case P_PER_NIV_FIN:
			log_trace(logger, "Personaje Finalizo Nivel %d y se desconecta.");
			break;
		default:
			log_trace(logger, "Planificador - Routine number %d dont exist.", routine);
			break;
	}
}

void rutinasOrquestador(int sockete, int routine, void* payload){
	switch(routine){
		//case P_NIV_MUERTE_PERSONAJE:
			//TODO LIBERAR RECURSOS
		//break;
		default:
			log_trace(logger, "Orquestador - Routine number %d dont exist.", routine);
		break;
	}
}

t_plataforma *configurar_plataforma(char *path){
	t_plataforma *plataforma = malloc(sizeof(t_plataforma));
	t_config *config = config_create(path);
	char *orquestador = config_get_string_value(config, "Orquestador");
	char** direccionPuerto = string_split(orquestador, ":");
	plataforma->direccion = string_duplicate(direccionPuerto[0]);
	plataforma->puerto = atoi(direccionPuerto[1]);
	free(direccionPuerto);
	free(orquestador);
	plataforma->deteccionInterbloqueo = config_get_int_value(config, "DeteccionInterbloqueo");
	plataforma->tiempoDeteccionInterbloqueo = config_get_int_value(config, "TiempoDeteccionInterbloqueo");
	plataforma->quantum = config_get_int_value(config, "Quantum");
	plataforma->retardo = config_get_int_value(config, "Retardo");
	config_destroy(config);
	return plataforma;
}
