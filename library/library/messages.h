/*
 * messages.h
 *
 *  Created on: 27/06/2013
 *      Author: utnso
 */

#ifndef MESSAGES_H_
#define MESSAGES_H_

//PROTOCOL MESSAGES

//PLATAFORMA
#define P_PLA_ACEPT_PER 50
#define P_PLA_MOV_PERMITIDO 51
#define P_PLA_ENVIO_NIVEL 52
#define P_PLA_ENVIO_PLANI 53

#define P_PLA_ACEPT_NIV 60

#define P_PLA_NIV_NO_EXITE 61

#define P_PLA_REC_LIB_RES 62

//NIVEL 100
#define P_NIV_ACEPT_PER 100
#define P_NIV_UBIC_RECURSO 101
#define P_NIV_OK_MOV 102
#define P_NIV_RECURSO_OK 103
#define P_NIV_RECURSO_BLOQUEO 104

#define P_NIV_CONNECT_ORQ 110

#define P_NIV_MUERTE_PERSONAJE 111;

#define P_NIV_RECURSOS_LIB 112;

//PERSONAJE 150
#define P_PER_CONECT_PLA 150
#define P_PER_CONECT_NIV 151
#define P_PER_LUGAR_RECURSO 152
#define P_PER_MOV 153
#define P_PER_PEDIR_RECURSO 154
#define P_PER_TURNO_FINALIZADO 155
#define P_PER_BLOQ_RECURSO 156

#define P_PER_CONECT_PLANI 157

#define P_PER_NIV_FIN 158

//STRUCTS
typedef struct {
	char direccion[16];
	int16_t puerto;
} __attribute__ ((__packed__)) t_direccionPuerto;

#endif /* MESSAGES_H_ */
