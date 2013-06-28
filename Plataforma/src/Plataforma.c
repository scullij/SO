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

#include <library/socket.h>
#include <library/protocol.h>

#include <commons/collections/list.h>

#define PUERTO 30000
#define BUFF_SIZE 1024

void rutines(int sockete, int routine, char* message);

void configurar_niveles();

t_list *niveles;

int main(void)
{
	configurar_niveles();

    fd_set master;    // master file descriptor list
    fd_set read_fds;  // temp file descriptor list for select()
    uint16_t fdmax;        // maximum file descriptor number

    uint16_t listener;     // listening socket descriptor
    uint16_t newfd;        // newly accept()ed socket descriptor

    char *buffer;    // buffer for client data
    int type;

	puts("Plataforma...");

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
                        printf("Plataforma: New connection on socket %d\n", newfd);
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
                    	printf("PLataforma: Socket %d, recibido: %s", i, buffer);
                    	rutines(i, type, buffer);
                    }
                } // END handle data from client
            } // END got new incoming connection
        } // END looping through file descriptors
    } // END for(;;)--and you thought it would never end!


    close(listener);
    free(buffer);
    return EXIT_SUCCESS;
}

void rutines(int sockete, int routine, char* message){
	//TODO Separar por Personaje o Nivel los switch, para manejar sus respectivas rutinas
	switch (routine) {
		case 1:
			printf("Message: %s, Socket: %d", message, &sockete);
			char* puertoNivel = list_get(niveles, (int)message);
			enviar(sockete, puertoNivel, 2);
			break;
		default:
			printf("Routine number %d dont exist.", &routine);
			break;
	}

}

void configurar_niveles(){
	niveles = list_create();
	list_add_in_index(niveles, 1, "30001");
	list_add_in_index(niveles, 2, "30002");
}
