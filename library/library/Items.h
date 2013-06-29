/*
 * Items.h
 *
 *  Created on: 29/06/2013
 *      Author: utnso
 */

#ifndef ITEMS_H_
#define ITEMS_H_

#include "gui.h"

void BorrarItem(ITEM_NIVEL** i, char id);
void restarRecurso(ITEM_NIVEL* i, char id);
void MoverPersonaje(ITEM_NIVEL* i, char personaje, int x, int y);
void CrearPersonaje(ITEM_NIVEL** i, char id, int x , int y);
void CrearCaja(ITEM_NIVEL** i, char id, int x , int y, int cant);
void CrearItem(ITEM_NIVEL** i, char id, int x, int y, char tipo, int cant);

#endif /* ITEMS_H_ */
