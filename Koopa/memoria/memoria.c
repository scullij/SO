#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "memoria.h"

t_log* logger;
t_list* _particiones;

t_particion* crear_particion (char id, int inicio, int tamanio, int index_particion);
int buscar_particion (char id);
int buscar_particion_best_fit (int tamanio);

t_memoria crear_memoria(int tamanio) {
	// Creo espacio de memoria
	t_memoria segmento = malloc (tamanio);

	// Initializo Logger
	logger = log_create("memoria.log", "memoria", "true", LOG_LEVEL_TRACE);
	log_trace(logger, "crear_memoria()");

	// Creo lista de particiones
	_particiones = list_create();

	// Creo particion vacia (tamanio total)
	t_particion* particionLibre = crear_particion ('\0', 0, tamanio, 0);
	particionLibre->libre = true;
	particionLibre->dato = segmento;

	log_trace(logger, "segmento direccion: %p", segmento);
	log_trace(logger, "segmento direccion maxima: %p", segmento + tamanio);
	log_trace(logger, "bloque inicial libre - dato: %p", particionLibre->dato);

	return segmento;
}

int almacenar_particion(t_memoria segmento, char id, int tamanio, char* contenido) {
	log_trace(logger, "almacenar_particion(tamanio: %d)", tamanio);
	log_trace(logger, contenido);

	// Busco la particion donde mejor cabe el tamanio (best fit)
	int index_BestFit = buscar_particion_best_fit(tamanio);

	if (index_BestFit == -1)
	{
		log_trace(logger, "=== NO BEST FIT ===");
		return -1;
	}

	t_particion* particionBestFit = list_get (_particiones, index_BestFit);

	log_trace(logger, string_from_format("BEST FIT FOUND: %i", particionBestFit->inicio));

	// Creo una nueva particion para almacenar la posicion
	t_particion* particionNueva = crear_particion(id, particionBestFit->inicio, tamanio, index_BestFit);
	memcpy(segmento + particionBestFit->inicio, contenido, tamanio);
	particionNueva->dato = segmento + particionBestFit->inicio;

	// Muevo el inicio de lo que sobra de espacio al tamanio de la nueva particion
	particionBestFit->inicio = particionBestFit->inicio + tamanio;
	particionBestFit->tamanio = particionBestFit->tamanio - tamanio;
	particionBestFit->dato = segmento + particionBestFit->inicio;

	if(particionBestFit->tamanio <= 0){
		eliminar_particion(segmento, particionBestFit->id);
	}

	log_trace(logger, "FIN almacenar_particion()");

	return 1;
}

int eliminar_particion(t_memoria segmento, char id) {
	log_trace(logger, "eliminar_particion()");

	int particionIndex = buscar_particion(id);
	t_particion* particion = list_get(_particiones, particionIndex);

	if (particion == NULL)
	{
		return -1;
	}

	particion->libre = true;

	return 0;
}

void liberar_particion (t_particion* particion) {
	free(particion);
}

void liberar_memoria(t_memoria segmento) {
	log_trace(logger, "liberar_memoria()");

	list_clean_and_destroy_elements(_particiones, (void*) liberar_particion);
	free(_particiones);
	free(segmento);
}


t_list* particiones(t_memoria segmento) {
	log_trace(logger, "particiones()");

	//int i;
	//t_list* list = list_create();


	//for(i=0; i < list_size(_particiones); i ++)
	//{
	//	t_particion* particion = list_get (_particiones, i);
	//	list_add(list, particion);
	//}

	t_list* list = list_take(_particiones, _particiones->elements_count);


	// IMPORTANTE: Koopa necesita una nueva lista, porque despues la libera de memoria
	return list;

}

t_particion* crear_particion (char id, int inicio, int tamanio, int index_particion) {
	t_particion* particion = malloc (sizeof(t_particion));
	particion->id = id;
	particion->inicio = inicio;
	particion->tamanio = tamanio;

	particion->dato = NULL;
	particion->libre = false;

	list_add_in_index(_particiones, index_particion, particion);
	return particion;
}


int buscar_particion (char id){
	int i = 0;

	for(i=0; i < list_size(_particiones); i ++)
	{
		t_particion* particion = list_get (_particiones, i);

		if (particion->id == id){
			return i;
		}
	}

	return -1;
}

int buscar_particion_best_fit (int tamanio){
	t_particion* bestFit = NULL;
	int index_bestFit = -1;
	int i = 0;

	log_trace(logger, "BEST FIT Algoritmo:");


	for(i=0; i < list_size(_particiones); i ++)
	{
		t_particion* particion = list_get (_particiones, i);
		log_trace(logger, string_from_format("Particion = %d - %d", i, particion->inicio));

		// Es una particion donde hay espacio disponible
		if (particion->libre && particion->tamanio >= tamanio)
		{
			// Encaja mejor que la anterior
			if (bestFit == NULL ||
				bestFit->tamanio > particion->tamanio)
			{
				bestFit = particion;
				index_bestFit = i;
			}
		}
	}

	return index_bestFit;
}
