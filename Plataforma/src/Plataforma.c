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
#include <commons/config.h>
#include <commons/log.h>

t_log* logger;

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

t_plataforma *plataforma;

t_plataforma *configurar_plataforma(char* path);

void rutinasOrquestador(int sockete, int routine, void* payload);
void rutinasPlanificador(int sockete, int routine, void* payload);
int *orquestador(void);
int *planificador(void);

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

	pthread_t thr_orquestador;

	pthread_create( &thr_orquestador, NULL, orquestador, NULL);

	pthread_join( thr_orquestador, NULL);

	free(plataforma);
	free(logger);
	return EXIT_SUCCESS;
}

int *orquestador(void){
	t_dictionary* niveles = dictionary_create();

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
                        FD_SET(newfd, &master); // add to master set
                        if (newfd > fdmax) {    // keep track of the max
                            fdmax = newfd;
                        }

                        //HANDSHAKE
						type = recibir(newfd, &buffer);
                        if(type == P_NIV_CONNECT_ORQ){
                        	t_nivelDireccionPuerto* nivelDirecionPuerto = malloc(sizeof(t_nivelDireccionPuerto));
                        	memcpy(nivelDirecionPuerto, buffer, sizeof(t_nivelDireccionPuerto));
							strcpy(nivelDirecionPuerto->direccion, direccion);
                        	log_trace(logger, "Orquestador: Nuevo nivel conectado: Direccion: %s Puerto: %d.\n", nivelDirecionPuerto->direccion, nivelDirecionPuerto->puerto);
                        	dictionary_put(niveles, nivelDirecionPuerto->nivel, nivelDirecionPuerto);
                        }else{
                        	log_trace(logger, "Orquestador: Nuevo personaje conectado.\n", newfd);
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

void rutinasOrquestador(int sockete, int routine, void* payload){
	pthread_t thr_pla;
	switch (routine) {
		case P_NIV_CONNECT_ORQ:
			//QUE CAGASO BOLUDO
			pthread_create( &thr_pla, NULL, planificador, NULL);
			break;
		default:
			log_error(logger, "Routine number %d dont exist.", routine);
			break;
	}
}

int *planificador(void){
	log_trace(logger, "PLanificador Nivel %d creado.", 1);
	sleep(5);
	log_trace(logger, "No muere por la referencia del thread liberada.");
	return EXIT_SUCCESS;
}

void rutinasPlanificador(int sockete, int routine, void* payload){
	//SOLO ESCUCHA PERSONAJES
	switch (routine) {
		case P_PER_CONECT_PLA:
            log_trace(logger, "ACEPTO PERSONAJE: en Socket: %u\n", sockete);
			//char* puertoNivel = list_get(niveles, (int)payload);
			enviar(sockete, P_PLA_ACEPT_PER, NULL, 0);
			enviar(sockete, P_PLA_MOV_PERMITIDO, plataforma->quantum, sizeof(int));
			break;
		case P_PER_TURNO_FINALIZADO:
			usleep(100*plataforma->retardo);
			enviar(sockete, P_PLA_MOV_PERMITIDO, plataforma->quantum, sizeof(int));
			break;
		default:
			log_error(logger, "Routine number %d dont exist.", routine);
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
