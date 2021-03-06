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

int personajesEnPlan;

//pthread_mutex_t mutex_planificacion = PTHREAD_MUTEX_INITIALIZER;

typedef struct{
	char personaje;
	int socket;
	char recurso;
} __attribute__ ((__packed__)) t_nodoPerPLa;

typedef struct {
	t_queue* rr;
	t_queue* bloqueados;
	t_nodoPerPLa* personajeActivo;
	pthread_mutex_t mutex;
} t_planificacionNodo;

t_dictionary* planificacion;

typedef struct {
	int16_t nivel;
	int16_t puerto;
	char direccion[16];
} __attribute__ ((__packed__)) t_nivelDireccionPuerto;

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

void rutinasOrquestador(int sockete, int routine, void* payload, t_log* logger);
void rutinasPlanificador(int sockete, int routine, void* payload, t_planificacionNodo* planificacion, int nivel, t_log* logger);
int *orquestador(void);
int *planificador(void* ptr);
void moverPersonajePLanificador(t_planificacionNodo* planificacion, int nivel, t_log* logger);

void lockAllThreads();
void unlockAllThreads();

int main(int argc, char *argv[]){
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
	return EXIT_SUCCESS;
}

int *orquestador(void){
	t_log* logger;
	logger = log_create("plataforma.log", "plataforma", true, LOG_LEVEL_TRACE);

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
		exit(EXIT_FAILURE);
	}

	log_trace(logger, "Orquestador: Listen %s, Port %u. \n", plataforma->direccion, plataforma->puerto);

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

                        	log_trace(logger, "Orquestador: Nuevo Nivel %d conectado: Direccion: %s Puerto: %d.", nivelDirecionPuerto->nivel, nivelDirecionPuerto->direccion, nivelDirecionPuerto->puerto);

                        	char* key = malloc(7);
                        	strcpy(key, string_from_format("nivel%d", nivelDirecionPuerto->nivel));

                        	lockAllThreads();
                        	t_planificacionNodo* planificacionNodo = malloc(sizeof(t_planificacionNodo));
                        	planificacionNodo->rr = queue_create();
                        	planificacionNodo->bloqueados = queue_create();
                        	planificacionNodo->personajeActivo = NULL;
                        	pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
                        	planificacionNodo->mutex = mutex;

                        	dictionary_put(planificacion, key, planificacionNodo);

                        	pthread_t thr_pla;
							t_plaThread* pla = malloc(sizeof(t_plaThread));
							pla->nivel = nivelDirecionPuerto->nivel;
							pla->puerto = puertoInicialPlanificador;
							pla->planificacionNodo = planificacionNodo;

							pthread_create( &thr_pla, NULL, planificador, pla);
							unlockAllThreads();

							char* newKey = malloc(3);
							sprintf(newKey,"%d", pla->nivel);
							dictionary_put(planificadorPuerto, newKey, pla->puerto);

				            puertoInicialPlanificador++;

                        	dictionary_put(niveles, key, nivelDirecionPuerto);
                        	free(key);
                        }else if(type == P_PER_CONECT_PLA){ //PERSONAJE
                        	personajesEnPlan++;
                        	log_trace(logger, "Orquestador: Personaje conectado.", newfd);
							char* key = malloc(7);
							strcpy(key, string_from_format("nivel%d", (*(int*)buffer)));
							t_nivelDireccionPuerto* nivelDireccionPuerto;
							nivelDireccionPuerto = dictionary_get(niveles, key);
							free(key);

							if(nivelDireccionPuerto == NULL){
								log_trace(logger, "Personaje pidio Nivel %d, que no exta conectado.", (*(int*)buffer));
								//enviar(newfd, P_PLA_NIV_NO_EXITE, nivelDireccionPuerto, sizeof(t_nivelDireccionPuerto));
								//free(nivelDireccionPuerto);
								break;
							}

							enviar(newfd, P_PLA_ENVIO_NIVEL, nivelDireccionPuerto, sizeof(t_nivelDireccionPuerto));
							int* puertoPLanificador = malloc(sizeof(int));

							char* newKey = malloc(3);
							sprintf(newKey,"%d", nivelDireccionPuerto->nivel);
							*puertoPLanificador = dictionary_get(planificadorPuerto, newKey);
							enviar(newfd, P_PLA_ENVIO_PLANI, puertoPLanificador, sizeof(int));
							free(newKey);
                        }else if(type == 159){
                        	log_trace(logger, "Koopa. PersonajesEnPLan %d", personajesEnPlan);
							if(personajesEnPlan == 0){
								//char *argv[] = {"/home/utnso/dev/tp-20131c-osgroup/Koopa/koopa", "koopa.config"};
								//execv("/home/utnso/dev/tp-20131c-osgroup/Koopa/koopa", argv);
								execl("/home/utnso/dev/tp-20131c-osgroup/Koopa/koopa", "/home/utnso/dev/tp-20131c-osgroup/Koopa/koopa", "/home/utnso/dev/tp-20131c-osgroup/Koopa/koopa.config", NULL);
							}
                        }
                    }
                } else {
                    // handle data from a client
                    if ((type = recibir(i,&buffer)) < 0) {
                        // got error or connection closed by client
                    	if(type==-1){
                    		close(i); // bye!
                    		FD_CLR(i, &master); // remove from master set
                    		log_trace(logger, "Alguien se desconecto.");
                    	}else{
                    		perror("Invalid Recv");
                    	}
                    } else {
                        // we got some data from a client
                    	rutinasOrquestador(i, type, buffer, logger);
                    }
                } // END handle data from client
            } // END got new incoming connection
        } // END looping through file descriptors
    } // END for(;;)--and you thought it would never end!


    close(listener);
    free(buffer);
    return EXIT_SUCCESS;
}

int *planificador(void* ptr){
	t_plaThread *pla;
	pla = (t_plaThread*)ptr;

	t_log* loggerPLanificador;
	char* logname = string_from_format("plani-%d", pla->nivel);
	loggerPLanificador = log_create(logname, logname, true, LOG_LEVEL_TRACE);

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
						rutinasPlanificador(i, type, buffer, pla->planificacionNodo, pla->nivel, loggerPLanificador);
					}
				} // END handle data from client
			} // END got new incoming connection
		} // END looping through file descriptors
	} // END for(;;)--and you thought it would never end!


	free(loggerPLanificador);
	free(logname);
	close(listener);

	return EXIT_SUCCESS;
}

void rutinasPlanificador(int sockete, int routine, void* payload, t_planificacionNodo* planificacion, int nivel, t_log* logger){
	pthread_mutex_lock( &planificacion->mutex );
	t_nodoPerPLa* new;
	//SOLO ESCUCHA PERSONAJES
	switch (routine) {
		case P_PER_CONECT_PLANI:
            log_trace(logger, "Planificador Nivel %d: Acepto personaje %c", nivel, ((char*)payload)[0]);
			enviar(sockete, P_PLA_ACEPT_PER, NULL, 0);

			new = malloc(sizeof(t_nodoPerPLa));
			new->personaje = (*(char*)payload);
			new->socket = sockete;

			queue_push(planificacion->rr, new);
			if(planificacion->personajeActivo == NULL){ // ES EL UNICO QUE ESTA POR AHORA
				moverPersonajePLanificador(planificacion, nivel, logger);
			}else{
				log_trace(logger, "Planificador Nivel %d, Encolo personaje: %c", nivel, new->personaje);
			}
			break;
		case P_PER_TURNO_FINALIZADO:
			queue_push(planificacion->rr, planificacion->personajeActivo);
			planificacion->personajeActivo = NULL;

			usleep(1000*plataforma->retardo);

			moverPersonajePLanificador(planificacion, nivel, logger);
			break;
		case P_PER_BLOQ_RECURSO:
			new = planificacion->personajeActivo;
			planificacion->personajeActivo = NULL;
			new->recurso = *(char*)payload;
			queue_push(planificacion->bloqueados, new);
			log_trace(logger, "Planificador Nivel %d, Bloqueo personaje: %c", nivel, new->personaje);

			moverPersonajePLanificador(planificacion, nivel, logger);

			break;
		case P_PER_NIV_FIN:
			free(planificacion->personajeActivo);
			planificacion->personajeActivo = NULL;
			log_trace(logger, "Personaje %c Finalizo Nivel %d y se desconecta.", (*(char*)payload), nivel);
			//Le da el ok de que lo saco del planificador
			enviar(sockete, 63, NULL, 0);

			personajesEnPlan--;

			moverPersonajePLanificador(planificacion, nivel, logger);

			break;
		default:
			log_trace(logger, "Planificador - Routine number %d dont exist.", routine);
			break;
	}
	pthread_mutex_unlock( &planificacion->mutex );
}

void moverPersonajePLanificador(t_planificacionNodo* planificacion, int nivel, t_log* logger){
	if(!queue_is_empty(planificacion->rr)){
		t_nodoPerPLa* new = (t_nodoPerPLa*)queue_pop(planificacion->rr);
		//log_trace(logger, "Planificador Nivel %d, Movimiento personaje %c, Quantum: %d", nivel, new->personaje, plataforma->quantum);
		log_trace(logger, "Planificador Nivel %d, Movimiento personaje %c, Quantum: %d. \n", nivel, new->personaje, plataforma->quantum);
		enviar(new->socket, P_PLA_MOV_PERMITIDO, &plataforma->quantum, sizeof(int));
		planificacion->personajeActivo = new;
	}
}

void rutinasOrquestador(int sockete, int routine, void* payload, t_log* logger){
	char recursosLiberados[30];
	switch(routine){
		case 112: //Liberar recursos nivel

		strncpy(recursosLiberados, (char*)payload, 15);

		int i = 1;
		lockAllThreads();
		t_planificacionNodo* planificadorNivel = dictionary_get(planificacion, string_from_format("nivel%c", recursosLiberados[0]));
		while(recursosLiberados[i] != NULL){
			int size = queue_size(planificadorNivel->bloqueados);
			int j;
			for (j = 0; j < size; j++) {
				t_nodoPerPLa* nodo = queue_pop(planificadorNivel->bloqueados);
				if(nodo->recurso == recursosLiberados[i]){
					log_trace(logger, "Personaje desbloqueado %c por recurso %c", nodo->personaje, nodo->recurso);
					queue_push(planificadorNivel->rr, nodo);
					recursosLiberados[i+15] = '1';
				}else{
					queue_push(planificadorNivel->bloqueados, nodo);
				}
			}
			i++;
		}
		enviar(sockete, 62, recursosLiberados, sizeof(recursosLiberados));
		recibir(sockete, &payload); //Termino de sumar instancias devueltas el nivel
		if(planificadorNivel->personajeActivo == NULL){
			moverPersonajePLanificador(planificadorNivel, (int)(recursosLiberados[0]-'0'), logger);
		}
		unlockAllThreads();

		break;
		case 113:
			lockAllThreads();
			t_planificacionNodo* planificadorNivel1 = dictionary_get(planificacion, string_from_format("nivel%c", *((char*)payload)));
			void* buffer;
			recibir(sockete, &buffer);
			log_trace(logger, "Nivel %c, mata personaje %c", *((char*)payload), *((char*)buffer));
			int size = queue_size(planificadorNivel1->bloqueados);
			int j;
			int socketLoco;
			for (j = 0; j < size; j++) {
				t_nodoPerPLa* nodo = queue_pop(planificadorNivel1->bloqueados);
				if(nodo->personaje != *((char*)buffer)){
					queue_push(planificadorNivel1->bloqueados, nodo);
				}else{
					socketLoco = nodo->socket;
					free(nodo);
				}
			}
			personajesEnPlan--;
			//MATO PJ
			enviar(socketLoco, 64, NULL, 0);
			//LE AVISO AL LV
			enviar(sockete, 64, NULL, 0);
			unlockAllThreads();
			break;
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
	//config_destroy(config);
	return plataforma;
}

void lockAllThreads(){
	void _bloquear_mutex(char* key, void* value){
		pthread_mutex_lock(&(((t_planificacionNodo*)value)->mutex));
	}
	dictionary_iterator(planificacion, _bloquear_mutex);
}

void unlockAllThreads(){
	void _desbloquear_mutex(char* key, void* value){
		pthread_mutex_unlock(&(((t_planificacionNodo*)value)->mutex));
	}
	dictionary_iterator(planificacion, _desbloquear_mutex);
}
