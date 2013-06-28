#include <stdlib.h>
#include "memoria.h"

t_log* logger;
t_list* _particiones;

t_particion* crear_particion (char id, int inicio, int tamanio);
t_particion* buscar_particion (char id);
t_particion* buscar_particion_best_fit (int tamanio);

t_memoria crear_memoria(int tamanio) {
	// Initializo Logger
	logger = log_create("memoria.log", "memoria", "true", LOG_LEVEL_TRACE);
	log_trace(logger, "crear_memoria()");

	// Creo lista de particiones
	_particiones = list_create();

	// Creo particion vacia (tamanio total)
	t_particion* particionLibre = crear_particion ('\0', 0, tamanio);
	particionLibre->libre = true;

	// Creo espacio de memoria
	t_memoria segmento = malloc (tamanio);
	return segmento;
}

int almacenar_particion(t_memoria segmento, char id, int tamanio, char* contenido) {
	log_trace(logger, "almacenar_particion()");
	log_trace(logger, contenido);

	// Busco la particion donde mejor cabe el tamanio (best fit)
	t_particion* particionBestFit = buscar_particion_best_fit(tamanio);

	if (particionBestFit == NULL)
	{
		log_trace(logger, "=== NO BEST FIT ===");
		return -1;
	}

	log_trace(logger, string_from_format("BEST FIT FOUND: %i", particionBestFit->inicio));

	// Creo una nueva particion para almacenar la posicion
	t_particion* particionNueva = crear_particion(id, particionBestFit->inicio, tamanio);
	string_append(&(particionNueva->dato), contenido);

	// Muevo el inicio de lo que sobra de espacio al tamanio de la nueva particion
	particionBestFit->inicio = particionBestFit->inicio + tamanio;
	particionBestFit->tamanio = particionBestFit->tamanio - tamanio;

	log_trace(logger, "FIN almacenar_particion()");

	return 1;
}

int eliminar_particion(t_memoria segmento, char id) {
	log_trace(logger, "eliminar_particion()");

	t_particion* particion = buscar_particion(id);

	if (particion == NULL)
	{
		return -1;
	}

	particion->libre = true;
	particion->dato = string_new();

	return 0;
}

void liberar_memoria(t_memoria segmento) {
	log_trace(logger, "liberar_memoria()");

	list_clean(_particiones);
	free(segmento);
}

t_list* particiones(t_memoria segmento) {
	log_trace(logger, "particiones()");

	int i;
	t_list* list = list_create();


	for(i=0; i < list_size(_particiones); i ++)
	{
		t_particion* particion = list_get (_particiones, i);
		list_add(list, particion);
	}

	// IMPORTANTE: Koopa necesita una nueva lista, porque despues la libera de memoria
	return list;

}

t_particion* crear_particion (char id, int inicio, int tamanio) {
	t_particion* particion = malloc (sizeof(t_particion));
	particion->id = id;
	particion->inicio = inicio;
	particion->tamanio = tamanio;

	particion->dato = string_new();
	particion->libre = false;

	list_add(_particiones, particion);
	return particion;
}


t_particion* buscar_particion (char id){
	int i = 0;

	for(i=0; i < list_size(_particiones); i ++)
	{
		t_particion* particion = list_get (_particiones, i);

		if (particion->id == id){
			return particion;
		}
	}

	return NULL;
}

t_particion* buscar_particion_best_fit (int tamanio){
	t_particion* bestFit = NULL;
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
			}
		}
	}

	return bestFit;
}




