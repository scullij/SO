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
#include <pthread.h>
#include <stdbool.h>

#include <commons/collections/list.h>
#include <commons/string.h>
#include <commons/log.h>
#include <commons/config.h>

#include <library/socket.h>
#include <library/messages.h>
#include <library/protocol.h>
#include <library/messages.h>

#include <library/Items.h>

#define MARCKED 1
#define DEADLOCK 2
#define BLOCKED 1
#define NONBLOCKED 2

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

//DEADLOCK

typedef struct{
	t_list* lista_pjs;
	t_nivel* nivel;
	t_log* logger;
	ITEM_NIVEL* items_nivel;
	int16_t orquestador;
}__attribute__ ((__packed__)) t_deadlock_params;

typedef struct{
	char id;
	t_list* resourse_list;
	uint8_t status;
	uint8_t deadlock_status;
	char resource_blocked;
	int16_t socket;
}__attribute__ ((__packed__)) t_character_lvl;

typedef struct {
	char resourse_id;
	uint16_t cantidad;
}__attribute__ ((__packed__)) t_pj_recursos;


pthread_mutex_t mutex_nivel = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_listapjs = PTHREAD_MUTEX_INITIALIZER;

t_list* personajes;
t_nivel* nivel;
ITEM_NIVEL* ListaItems;
t_log* logger;

//DEADLOCK

uint16_t listener;     // listening socket descriptor
fd_set master;    // master file descriptor list

void rutines(int sockete, int routine, void* payload, int orquestador);
t_nivel *configurar_nivel(char* path);
void iterar_recurso(t_list* self, void(*closure)(t_recurso*));
void setCantidadRecursos(ITEM_NIVEL** ListaItems, char id, int cantidad);

//Deadlock
void personaje_matar(t_character_lvl* character_to_kill, int16_t orquestador, int muerte);
void *verificar_interbloqueo(void* args);

int main(int argc, char *argv[]) {
	char* path = malloc(0);
	if(argc < 2){
		puts("Ingrese el path de configuracion:");
		scanf("%s", path);
	}else {
		path = string_duplicate(argv[1]);
	}

	personajes = list_create();

	nivel = configurar_nivel(path);

	char* logName = string_from_format("%s.log", nivel->nombre);
	logger = log_create(logName, nivel->nombre, false, LOG_LEVEL_TRACE);

	//Nos conectamos al orquestador para pasarle info del nivel
	int16_t orquestador = create_and_connect(nivel->direccionOrq, nivel->puertoOrq);
	if(orquestador == -1){
		perror("orquestador");
	}

	pthread_t thr_deadlock;
	pthread_create(&thr_deadlock, NULL, verificar_interbloqueo,(void*)&orquestador);

	t_nivelDireccionPuerto* nivelPuerto = malloc(sizeof(t_nivelDireccionPuerto));

	nivelPuerto->nivel = *(nivel->nombre+5) - '0';
	nivelPuerto->puerto = nivel->puerto;
	enviar(orquestador, P_NIV_CONNECT_ORQ, nivelPuerto, sizeof(nivelPuerto));

    fd_set read_fds;  // temp file descriptor list for select()
    uint16_t fdmax;        // maximum file descriptor number

    unsigned int newfd;        // newly accept()ed socket descriptor

    void *buffer;    // buffer for client data
    int type;

	//DRAWING PAPA
	int rows, cols;
	nivel_gui_inicializar();
    nivel_gui_get_area_nivel(&rows, &cols);

    void _crear_recurso(t_recurso* element){
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
                    	rutines(i, type, buffer, orquestador);
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

void rutines(int sockete, int routine, void* payload, int orquestador){
	t_posicion* posicion = malloc(sizeof(t_posicion));
	posicion->x = -1;
	t_character_lvl* personaje;
	//SOLO ESCUCHA PERSONAJES
	switch (routine) {
		case P_PER_CONECT_NIV:
			log_trace(logger, "Acepto Personaje: en Socket: %u", sockete);
			CrearPersonaje(&ListaItems, (*(char*)payload), 1, 1);

			pthread_mutex_lock(&mutex_listapjs);
			personaje = malloc(sizeof(t_character_lvl));
			personaje->id = (*(char*)payload);
			personaje->socket = sockete;
			personaje->deadlock_status = 0;
			personaje->resource_blocked = 0;
			personaje->status = NONBLOCKED;
			personaje->resourse_list = list_create();
			list_add(personajes, personaje);
			pthread_mutex_unlock(&mutex_listapjs);

			enviar(sockete, P_NIV_ACEPT_PER, NULL, 0);
			break;
		case P_PER_LUGAR_RECURSO:
			log_trace(logger, "Pedido posicion recurso: %c", (*(char*)payload));
			void _encontrar_recurso(t_recurso* element){
				if(element->simbolo == (*(char*)payload)){
					posicion->x = element->x;
					posicion->y = element->y;
				}
			}
    		pthread_mutex_lock(&mutex_nivel);
			iterar_recurso(nivel->recursos, _encontrar_recurso);
    		pthread_mutex_unlock(&mutex_nivel);

			enviar(sockete, P_NIV_UBIC_RECURSO, posicion, sizeof(posicion));
			break;
		case P_PER_MOV:
			posicion = (t_posicion*)payload;
			log_trace(logger, "Personaje a mover %c, posicionX %d posicionY %d", posicion->personaje, posicion->x, posicion->y);
			log_trace(logger, "Movimiento personaje: x: %d, y: %d\n", posicion->x, posicion->y);
			MoverPersonaje(ListaItems, posicion->personaje, posicion->x, posicion->y);
			enviar(sockete, P_NIV_OK_MOV, NULL, 0);
			break;
		case P_PER_PEDIR_RECURSO:
			posicion = (t_posicion*)payload;
			pthread_mutex_lock(&mutex_nivel);
			pthread_mutex_lock(&mutex_listapjs);
			int i;
			for(i=0;i<list_size(nivel->recursos);i++){
				t_recurso* element = list_get(nivel->recursos,i);
		    	if(element->x == posicion->x && element->y == posicion->y){

		    		bool _encontrar_personaje_por_id(void* asd){
		    			return ((t_character_lvl*)asd)->id == posicion->personaje;
		    		}
		    		personaje = list_find(personajes, _encontrar_personaje_por_id);
					personaje->resource_blocked = 0;
					personaje->status = NONBLOCKED;

		    		if(element->instancias > 0){
		    			bool _encontrar_recurso(void* asd){
		    				return ((t_pj_recursos*)asd)->resourse_id == element->simbolo;
		    			}
		    			t_pj_recursos* recurso = list_find(personaje->resourse_list, _encontrar_recurso);
		    			if(recurso == NULL){
		    				t_pj_recursos* recurso = malloc(sizeof(t_pj_recursos));
		    				recurso->cantidad = 1;
		    				recurso->resourse_id = element->simbolo;
		    				list_add(personaje->resourse_list, recurso);
		    			}else{
		    				recurso->cantidad++;
		    			}

		    			element->instancias--;
		    			restarRecurso(ListaItems, element->simbolo);
		    			log_trace(logger, "Recurso a asignar %c", element->simbolo);
		    			enviar(sockete, P_NIV_RECURSO_OK, NULL, 0);
		    		}else {
		    			personaje->status = BLOCKED;
		    			personaje->resource_blocked = element->simbolo;
		    			log_trace(logger, "Recurso bloqueado %c", element->simbolo);
		    			enviar(sockete, P_NIV_RECURSO_BLOQUEO, NULL, 0);
					}
				}
		    }
			pthread_mutex_unlock(&mutex_listapjs);
    		pthread_mutex_unlock(&mutex_nivel);
			break;
		case P_PER_NIV_FIN:
			log_trace(logger, "Personaje %c termina.", *((char*)payload));
    		pthread_mutex_lock(&mutex_listapjs);
    		pthread_mutex_lock(&mutex_nivel);
    		bool _encontrar_personaje_por_id(void* asd){
				return ((t_character_lvl*)asd)->id == *((char*)payload);
			}
			personaje = list_find(personajes, _encontrar_personaje_por_id);
    		personaje_matar(personaje, orquestador, 0);
    		free(list_remove_by_condition(personajes, _encontrar_personaje_por_id));
    		pthread_mutex_unlock(&mutex_nivel);
    		pthread_mutex_unlock(&mutex_listapjs);
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

void setCantidadRecursos(ITEM_NIVEL** ListaItems, char id, int cantidad) {

        ITEM_NIVEL * temp;
        temp = *ListaItems;

        while ((temp != NULL) && (temp->id != id)) {
			temp = temp->next;
        }
        if ((temp != NULL) && (temp->id == id)) {
			if ((temp->item_type) && (temp->quantity >= 0)) {
				temp->quantity = cantidad;
			}
        }
}

void *verificar_interbloqueo(void* args){
	//t_deadlock_params deadlock_params = *((t_deadlock_params *)args);
	int16_t orquestador = *((int16_t*)args);

	while(1){
		sleep(nivel->tiempoChequeoDeadlock);
		log_trace(logger, "Deadlock check started.");

		pthread_mutex_lock(&mutex_listapjs);
		pthread_mutex_lock(&mutex_nivel);

		int _character_is_blocked(t_character_lvl* character_blocked){
			return character_blocked->status == BLOCKED;
		}

		t_list* personajes_bloqueados = list_filter(personajes,(void*)_character_is_blocked);
		/*Si la cantidad de personajes bloqueados es mayor a 0, entonces hago todoo esto*/
		if(list_size(personajes_bloqueados)>1){
			int cantidad_recursos = list_size(nivel->recursos);
			t_recurso vector_disponibles[cantidad_recursos];
			/*Inicializo el vector de disponibles*/
			int i;
			for(i=0; i<cantidad_recursos; i++){
				t_recurso* box_recurso = list_get(nivel->recursos, i);
				vector_disponibles[i].instancias = box_recurso->instancias;
				vector_disponibles[i].simbolo = box_recurso->simbolo;
			}

			/*Verifico por cada personaje que no tenga recursos asignados*/
			for(i=0; i<list_size(personajes_bloqueados); i++){
				t_character_lvl* personaje = list_get(personajes_bloqueados, i);

				/*Verifico que no tenga recursos asignados. Si no tiene recursos, lo marco*/
				int recursos_nulos = list_size(personaje->resourse_list);
				if(recursos_nulos == cantidad_recursos){
					/*Si todos los recursos del nivel estan en 0 para el personaje seleccionado lo marco*/
					personaje->deadlock_status = MARCKED;
				}else{
					personaje->deadlock_status = DEADLOCK;
				}
			}

			int _character_unmarcked(t_character_lvl* character_unmarcked){
				return character_unmarcked->deadlock_status != MARCKED;
			}
			t_list* personajes_no_marcados = list_filter(personajes_bloqueados,(void*) _character_unmarcked);

			/*Tomo los personajes no marcados*/
			int j=0;
			while(j<list_size(personajes_no_marcados)){
				/*Por cada uno de estos personajes busco alguno que el recurso que lo bloqueo lo tenga el vector de disponibles*/
				t_character_lvl* personaje_no_marcado = list_get(personajes_no_marcados,j);
				j++;
				for(i=0; i<cantidad_recursos; i++){
					if(personaje_no_marcado->resource_blocked == vector_disponibles[i].simbolo){
						/*Pregunto si la cantidad de instancias del recurso es mayor a 0, entonces se lo doy*/
						if(vector_disponibles[i].instancias > 0){
							personaje_no_marcado->deadlock_status = MARCKED;
							vector_disponibles[i].instancias++;
							/*Vuelvo a setiar i en 0 para que vuelva a recorrer toda la lista*/
							j=0;
						}
						personajes_no_marcados = list_filter(personajes_bloqueados,(void*)_character_unmarcked);
						break;
					}
				}
			}
			/*Filtro los personajes que no fueron marcados y estos son los que estan en deadlock*/
			int _has_deadlock(t_character_lvl* character_not_marcked){
				return character_not_marcked->deadlock_status == DEADLOCK;
			}
			t_list* personajes_en_deadlock = list_filter(personajes,(void*)_has_deadlock);
			log_info(logger, "Cantidad de Personajes en Deadlock: %d", list_size(personajes_bloqueados));

			/*Debo volver a poner en 0 todos los status de los PJs*/
			for(i=0;i<list_size(personajes);i++){
				t_character_lvl* character_lvl = list_get(personajes,i);
				character_lvl->deadlock_status = 0;
			}

			if (nivel->recovery == 1 && list_size(personajes_en_deadlock) >= 2){
				//Deshabilito los eventos sobre el socket_orquestador//
				FD_CLR(listener, &master);
				/*event.data.fd = socket_orquestador.socketfd;
				event.events = EPOLLIN | EPOLLET;
				epoll_ctl(epoll_instance, EPOLL_CTL_DEL, socket_orquestador.socketfd, &event);*/

				log_trace(logger, "Mando al orquestador personaje a matar.");
				/*Le aviso que se detecto un interbloqueo*/
				/*Le envio el id del primer pj que esta en deadlock (este es el criterio) Para que lo elimine*/
				t_character_lvl *character_in_deadlock = list_remove(personajes_en_deadlock,0);
				char id_personaje=character_in_deadlock->id;

				/*Mato al proceso*/
				log_trace(logger,"El Personaje %c fue eliminado por deadlock",id_personaje);

				enviar(orquestador, 113, nivel->nombre+5, sizeof(char));
				enviar(orquestador, 113, &id_personaje, sizeof(char));

				//Recibo que el PJ fue eliminado
				void* buffer;
				recibir(orquestador, &buffer);

				//Libero recursos ahora
				personaje_matar(character_in_deadlock,orquestador, 1);
				int _search_character_in_main_list(t_character_lvl* character){
					return character->socket == character_in_deadlock->socket;
				}
				//free(character_in_deadlock);
				free(list_remove_by_condition(personajes, (void*)_search_character_in_main_list));
				/*Dibujo en pantalla*/
				nivel_gui_dibujar(ListaItems);


				//Habilito los eventos sobre el socket_orquestador//
				FD_SET(listener, &master);
				/*event.data.fd = socket_orquestador.socketfd;
				event.events = EPOLLIN | EPOLLET;
				epoll_ctl(epoll_instance, EPOLL_CTL_ADD, socket_orquestador.socketfd, &event);*/

			}
		}

		/*Deslockeo el mutex listapjs*/
		pthread_mutex_unlock(&mutex_listapjs);
		/*Deslockeo el nivel porque ya verifique lo que necesitaba*/
		pthread_mutex_unlock(&mutex_nivel);
	}
}

void personaje_matar(t_character_lvl* character_to_kill, int16_t orquestador, int muerte){
	t_list* lista_recursos= character_to_kill->resourse_list;

	/*Envio cada uno de los recursos*/
	char recursosLiberados[30];
	strncpy(recursosLiberados, nivel->nombre+5, 1);
	int i = 1;
	while(!list_is_empty(lista_recursos)){
		t_pj_recursos* recurso=list_remove(lista_recursos,0);
		int l;
		for (l = 0; l < recurso->cantidad; ++l) {
			recursosLiberados[i] = recurso->resourse_id;
			i++;
		}
		free(recurso);
	}
	list_clean(lista_recursos);
	close(character_to_kill->socket);
	FD_CLR(character_to_kill->socket, &master); // remove from master set

	recursosLiberados[i] = '\0';
	log_trace(logger, "Recursos a liberar al matar personaje %s", recursosLiberados);
	//P_NIV_RECURSOS_LIBERADOS
	enviar(orquestador, 112, recursosLiberados, sizeof(recursosLiberados));
	void* payload;
	recibir(orquestador, &payload);
	strncpy(recursosLiberados, (char*)payload, 15);

	void _liberar_recurso(void* el){
		int j=1;
		while(recursosLiberados[j] != NULL){
			if(((t_recurso*)el)->simbolo == recursosLiberados[j]){
				if(recursosLiberados[j+15] != '1'){
					//sumarRecurso(ListaItems, ((t_recurso*)el)->simbolo);
					setCantidadRecursos(&ListaItems, ((t_recurso*)el)->simbolo, ((t_recurso*)el)->instancias+1);
				}
				((t_recurso*)el)->instancias++;
			}
			j++;
		}

	}
	list_iterate(nivel->recursos, _liberar_recurso);

	enviar(orquestador, 114, NULL, 0);
}
