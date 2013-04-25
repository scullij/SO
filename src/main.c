/*
 ============================================================================
 Name        : main.c
 Author      : OSGroup
 Version     :
 Copyright   : Your copyright notice
 Description : TP
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <commons/temporal.h> // Shared library configurada

int main(void) {
	char* tiempo = temporal_get_string_time();
	puts(tiempo);
	free(tiempo);
	return EXIT_SUCCESS;
}
