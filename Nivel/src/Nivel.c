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

#include <commons/collections/list.h>
#include <library/socket.h>
#include <library/messages.h>
#include <library/protocol.h>
#include <commons/string.h>

#include <library/Items.h>

void rutines(int sockete, int routine, void* payload);

#define DIRECCION "127.0.0.1"
#define PUERTO 30005

typedef struct {
	int16_t x;
	int16_t y;
} __attribute__ ((__packed__)) t_posicion;

typedef struct {
	char* nombre;
	char simbolo;
	int instancias;
	int x;
	int y;
} t_recurso;

typedef struct {
	//TODO Nombre es string
	int nombre;
	t_recurso recursos[3];
	char* orquestador;
	int deadlock;
	char recovery;
} t_nivel;

t_nivel* niv = NULL;
ITEM_NIVEL* ListaItems = NULL;

int main(void) {

    fd_set master;    // master file descriptor list
    fd_set read_fds;  // temp file descriptor list for select()
    uint16_t fdmax;        // maximum file descriptor number

    uint16_t listener;     // listening socket descriptor
    unsigned int newfd;        // newly accept()ed socket descriptor

    void *buffer;    // buffer for client data
    int type;

	puts("Nivel");

	niv = malloc(sizeof(t_nivel));
	niv->recursos[0].simbolo = 'H';
	niv->recursos[0].x = 10;
	niv->recursos[0].y = 20;
	niv->recursos[0].instancias = 3;

	niv->recursos[1].simbolo = 'F';
	niv->recursos[1].x = 5;
	niv->recursos[1].y = 14;
	niv->recursos[1].instancias = 2;

	niv->recursos[2].simbolo = 'M';
	niv->recursos[2].x = 5;
	niv->recursos[2].y = 3;
	niv->recursos[2].instancias = 5;

	//DRAWING PAPA
	int rows, cols;
	nivel_gui_inicializar();
    nivel_gui_get_area_nivel(&rows, &cols);

	CrearCaja(&ListaItems, niv->recursos[0].simbolo, niv->recursos[0].x, niv->recursos[0].y, niv->recursos[0].instancias);
	CrearCaja(&ListaItems, niv->recursos[1].simbolo, niv->recursos[1].x, niv->recursos[1].y, niv->recursos[1].instancias);
	CrearCaja(&ListaItems, niv->recursos[2].simbolo, niv->recursos[2].x, niv->recursos[2].y, niv->recursos[2].instancias);
	nivel_gui_dibujar(ListaItems);

	//LISTENER
	listener = create_and_listen(PUERTO);
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
                    newfd = accept_connection(listener);
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
	int i = 0;
	//SOLO ESCUCHA PERSONAJES
	switch (routine) {
		case 151:
			printf("ACEPTO PERSONAJE: en Socket: %u\n", sockete);
			CrearPersonaje(&ListaItems, '@', 1, 1);
			//char* puertoNivel = list_get(niveles, (int)payload);
			enviar(sockete, 100, NULL, 0);
			break;
		case 152:
//			printf("PEDIDO LUGAR RECURSO PERSONAJE: en Socket: %u\n", sockete);
			for(i=0; i < 3; i++){
				if(niv->recursos[i].simbolo == (*(char*)payload)){
					posicion->x = niv->recursos[i].x;
					posicion->y = niv->recursos[i].y;
					break;
				}
			}
			enviar(sockete, 101, posicion, sizeof(posicion));
			break;
		case 153:
			posicion = (t_posicion*)payload;
//			printf("MOVIMIENTO PERSONAJE EN: x: %d, y: %d\n", posicion->x, posicion->y);
			MoverPersonaje(ListaItems, '@', posicion->x, posicion->y);
			enviar(sockete, 102, NULL, 0);
			break;
		case 154:
			posicion = (t_posicion*)payload;
//			puts("PEDIDO RECURSO");
			//TODO VALIDAR POSICION CORRECTA PERSONAJE PARA PEDIR RECURSO
			for(i=0; i < 3; i++){
				if(niv->recursos[i].x == posicion->x && niv->recursos[i].y == posicion->y){
					restarRecurso(ListaItems, niv->recursos[i].simbolo);
				}
			}
			enviar(sockete, 103, NULL, 0);
			break;
		default:
			printf("Routine number %d dont exist.", routine);
			break;
	}
	nivel_gui_dibujar(ListaItems);
	fflush(stdout);
}

//int drawing(void) {
//
//	MoverPersonaje(ListaItems, '@', 1, 1);
//
//	restarRecurso(ListaItems, 'H');
//	BorrarItem(&ListaItems, '#'); //si chocan, borramos uno (!)
//
//	nivel_gui_dibujar(ListaItems);
//
//	BorrarItem(&ListaItems, '#');
//	BorrarItem(&ListaItems, '@');
//	BorrarItem(&ListaItems, 'H');
//	BorrarItem(&ListaItems, 'M');
//	BorrarItem(&ListaItems, 'F');
//
//	nivel_gui_terminar();
//
//	return EXIT_SUCCESS;
//}

