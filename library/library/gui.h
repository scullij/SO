/*
 * gui.h
 *
 *  Created on: 29/06/2013
 *      Author: utnso
 */

#ifndef GUI_H_
#define GUI_H_

#define PERSONAJE_ITEM_TYPE 0
#define RECURSO_ITEM_TYPE 1

struct item {
	char id;
	int posx;
	int posy;
	char item_type; // PERSONAJE o CAJA_DE_RECURSOS
	int quantity;
	struct item *next;
};

typedef struct item ITEM_NIVEL;

int nivel_gui_dibujar(ITEM_NIVEL* items);
int nivel_gui_terminar(void);
int nivel_gui_inicializar(void);
int nivel_gui_get_area_nivel(int * rows, int * cols);


#endif /* GUI_H_ */
