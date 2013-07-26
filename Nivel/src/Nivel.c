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
#include <curses.h>
#include <unistd.h>

#include <commons/collections/list.h>
#include <commons/string.h>
#include <commons/log.h>
#include <commons/config.h>

#include <library/socket.h>
#include <library/messages.h>
#include <library/protocol.h>
#include <library/messages.h>

#include <library/Items.h>

t_log* logger;

typedef struct {
	int16_t x;
	int16_t y;
	char personaje;
} __attribute__ ((__packed__)) t_posicion;

typedef struct {
	int16_t nivel;
	int16_t puerto;
	char direccion[16];
} __attribute__ ((__packed__)) t_nivelDireccionPuerto;

typedef struct {
	char* nombre;
	char simbolo;
	int instancias;
	int x;
	int y;
} t_recurso;

typedef struct {
	//TODO Nombre es string
	char *nombre;
	t_list *recursos;
	char* direccionOrq;
	uint16_t puertoOrq;
	uint16_t puerto;
	int tiempoChequeoDeadlock;
	int recovery;
} t_nivel;

t_nivel* nivel;
ITEM_NIVEL* ListaItems;

void rutines(int sockete, int routine, void* payload);
t_nivel *configurar_nivel(char* path);
void iterar_recurso(t_list* self, void(*closure)(t_recurso*));

int main(int argc, char *argv[]) {
	logger = log_create("nivel.log", "nivel", false, LOG_LEVEL_TRACE);
	char* path = malloc(0);
	if(argc < 2){
		puts("Ingrese el path de configuracion:");
		scanf("%s", path);
	}else {
		path = string_duplicate(argv[1]);
	}

	nivel = configurar_nivel(path);

	//Nos conectamos al orquestador para pasarle info del nivel
	uint16_t orquestador = create_and_connect(nivel->direccionOrq, nivel->puertoOrq);
	if(orquestador == -1){
		perror("orquestador");
	}

	t_nivelDireccionPuerto* nivelPuerto = malloc(sizeof(t_nivelDireccionPuerto));
	nivelPuerto->nivel = 1;
	nivelPuerto->puerto = nivel->puerto;
	enviar(orquestador, P_NIV_CONNECT_ORQ, nivelPuerto, sizeof(nivelPuerto));

	fd_set master;    // master file descriptor list
    fd_set read_fds;  // temp file descriptor list for select()
    uint16_t fdmax;        // maximum file descriptor number

    uint16_t listener;     // listening socket descriptor
    unsigned int newfd;        // newly accept()ed socket descriptor

    void *buffer;    // buffer for client data
    int type;

	//DRAWING PAPA
	int rows, cols;
	nivel_gui_inicializar();
    nivel_gui_get_area_nivel(&rows, &cols);

    void _crear_recurso(t_recurso* element){
    	//log_trace(logger, "Recurso: %c, %d, %d, %d", element->simbolo, element->instancias, element->x, element->y);
    	CrearCaja(&ListaItems, element->simbolo, element->x, element->y, element->instancias);
    }
    iterar_recurso(nivel->recursos, _crear_recurso);
    nivel_gui_dibujar(ListaItems);

	//LISTENER
	listener = create_and_listen(nivel->puerto);
	if(listener == -1){
		perror("listener");
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
                        perror("accept");
                    } else {
                        FD_SET(newfd, &master); // add to master set
                        if (newfd > fdmax) {    // keep track of the max
                            fdmax = newfd;
                        }
//                        printf("Nivel: New connection on socket %u.\n", newfd);
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
                    	//TODO: RUTINA CON EL TYPE
//                    	printf("Nivel: Socket %d, recibido: %u\n", i, type);
                    	rutines(i, type, buffer);
                    }
                } // END handle data from client
            } // END got new incoming connection
        } // END looping through file descriptors
    } // END for(;;)--and you thought it would never end!

    nivel_gui_terminar();
    close(listener);
    free(buffer);
    return EXIT_SUCCESS;
}

void rutines(int sockete, int routine, void* payload){
	t_posicion* posicion = malloc(sizeof(t_posicion));
	posicion->x = -1;
	//SOLO ESCUCHA PERSONAJES
	switch (routine) {
		case P_PER_CONECT_NIV:
			log_trace(logger, "Acepto Personaje: en Socket: %u\n", sockete);
			CrearPersonaje(&ListaItems, (*(char*)payload), 1, 1);
			//char* puertoNivel = list_get(niveles, (int)payload);
			enviar(sockete, P_NIV_ACEPT_PER, NULL, 0);
			break;
		case P_PER_LUGAR_RECURSO:
			log_trace(logger, "Pedido posicion recurso: %c", (*(char*)payload));
			void _encontrar_recurso(t_recurso* element){
				log_trace(logger, "BUscando recurso: %c == %c", element->simbolo, (*(char*)payload));
				if(element->simbolo == (*(char*)payload)){
					posicion->x = element->x;
					posicion->y = element->y;
				}
			}
			iterar_recurso(nivel->recursos, _encontrar_recurso);

			enviar(sockete, P_NIV_UBIC_RECURSO, posicion, sizeof(posicion));
			break;
		case P_PER_MOV:
			posicion = (t_posicion*)payload;
//			log_trace(logger, "MOVIMIENTO PERSONAJE EN: x: %d, y: %d\n", posicion->x, posicion->y);
			MoverPersonaje(ListaItems, posicion->personaje, posicion->x, posicion->y);
			enviar(sockete, P_NIV_OK_MOV, NULL, 0);
			break;
		case P_PER_PEDIR_RECURSO:
			posicion = (t_posicion*)payload;
			//TODO VALIDAR POSICION CORRECTA PERSONAJE PARA PEDIR RECURSO
		    void _restar_recurso(t_recurso* element){
		    	if(element->x == posicion->x && element->y == posicion->y){
		    		if(element->instancias > 0){
		    			restarRecurso(ListaItems, element->simbolo);
		    			enviar(sockete, P_NIV_RECURSO_OK, NULL, 0);
		    		}else {
		    			enviar(sockete, P_NIV_RECURSO_BLOQUEO, NULL, 0);
					}
				}
		    }
			iterar_recurso(nivel->recursos, _restar_recurso);
			break;
		case P_PER_NIV_FIN:
			//TODO LIMPIAR RECURSOS LIBERADO POR EL PERSONAJE
			break;
		default:
			log_trace(logger, "Routine number %d dont exist.", routine);
			break;
	}
	nivel_gui_dibujar(ListaItems);
	fflush(stdout);
}

t_nivel *configurar_nivel(char* path){
	t_nivel *nivel = malloc(sizeof(t_nivel));
	t_config *config = config_create(path);
	char *orquestador = config_get_string_value(config, "Orquestador");
	char** direccionPuerto = string_split(orquestador, ":");
	nivel->direccionOrq = string_duplicate(direccionPuerto[0]);
	nivel->puertoOrq = atoi(direccionPuerto[1]);
	free(direccionPuerto);
	free(orquestador);
	nivel->puerto = config_get_int_value(config, "Puerto");
	nivel->nombre = string_duplicate(config_get_string_value(config, "Nombre"));
	nivel->tiempoChequeoDeadlock = config_get_int_value(config, "TiempoChequeoDeadlock");
	nivel->recovery = config_get_int_value(config, "Recovery");

	//Recursos
	nivel->recursos = list_create();
	int i;
	for (i = 1; i <= config_keys_amount(config)-5; i++) {
		if(config_has_property(config, string_from_format("Caja%d",i))){
			t_recurso *recurso = malloc(sizeof(t_recurso));
			char **caja = config_get_array_value(config, string_from_format("Caja%d",i));
			//recurso->nombre = string_duplicate(caja[0]);

			recurso->nombre = malloc(strlen(caja[0]));
			strncpy(recurso->nombre, caja[0], strlen(caja[0])+1);

			recurso->simbolo = caja[1][0];
			recurso->instancias = atoi(caja[2]);
			recurso->x = atoi(caja[3]);
			recurso->y = atoi(caja[4]);
			list_add(nivel->recursos, recurso);
			free(caja);
		}
	}

	//TODO: LCTMAB - Me rompe el nombre y el simbolo, si libero el config
	//config_destroy(config);

	return nivel;
}

void iterar_recurso(t_list* self, void(*closure)(t_recurso*)) {
	t_link_element *element = self->head;
	while (element != NULL) {
		closure(element->data);
		element = element->next;
	}
}

