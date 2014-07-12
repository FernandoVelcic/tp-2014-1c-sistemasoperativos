#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "Segmento.h"
#include "Consola.h"

#include "commons/collections/list.h"
#include "commons/log.h"
#include "commons/sockets.h"

#ifndef MEMORIA_H_
#define MEMORIA_H_

#define WORSTFIT 1
#define FIRSTFIT 0
#define SEGMENTOVACIO 0
#define PorCONSOLA 0
#define PorARCHIVO 1

bool segmentoEsAnterior( void * seg1, void * seg2 );
t_list * crearListaEspacioDisponible();

Segmento * crearYllenarSegmento( uint32_t tamanio, void * segmento );
Segmento * crearSegmento		( uint32_t tamanio );
Segmento * crearSegmentoFirstFit	(  uint32_t tamanio );
Segmento * crearSegmentoWorstFit(  uint32_t tamanio );
Segmento * buscarSegmentoEnTabla( uint32_t idSeg);
void borrarSegmento( Segmento * segmentoABorrar );
void moverSegmento(Segmento * segmento, uint32_t posicion) ;
uint32_t tamanioSegmento(Segmento * segmento);

void ordenarTablaSegmentos();
void compactar();
uint32_t memoriaOcupada();
uint32_t memoriaLibre();

bool solicitarPosicionDeMemoria( uint32_t base, uint32_t offset, uint32_t tamanio);
void imprimirBytes( uint32_t base, uint32_t offset, uint32_t tamanio, char porDondeImprimo);
void mostrarCaracteres( uint32_t cantidad, unsigned char * mem, char porDondeImprimo);
bool escribirPosicionDeMemoria( uint32_t base, uint32_t offset, uint32_t tamanio, uint32_t  buffer[]);
bool chequearSegmentatiosFault( Segmento * segmento, uint32_t offset, uint32_t tamanio);



#endif /* MEMORIA_H_ */
