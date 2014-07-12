#ifndef CONSOLA_H_
#define CONSOLA_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "commons/log.h"
#include "commons/string.h"
#include "commons/collections/list.h"

#include "Segmento.h"
#include "Programa.h"
#include <pthread.h>

#define PorCONSOLA 0
#define PorARCHIVO 1

void * iniciarConsola(void * params);
void operacionesConSegmentos();
void modificarAlgoCreacionSegmentos();
void modificarRetardoUMV();
void generarDump();
void requisitosOperacionSegmento( char operacion);
void solicitarPosicion( uint32_t base, uint32_t offset, uint32_t tamanio);
void escribirPosicion();
void imprimirMemoria();
void printSegmentos(t_list * segmentos, char porDondeImprimo);
void printSegmentosHeaders(char porDondeImprimo);
void printSegmentosPorPrograma();
void printTodosSegmentos();
void printSegmento(Segmento * segmento, char porDondeImprimo);
void buscarProgramaEImprimirSegmentos();
void printEspacioLibre(uint32_t inicioEspacio, uint32_t finEspacio, char porDondeImprimo);
bool verificarRequisitos( uint32_t programa, uint32_t base);

void imprimirSegmentosDe(Programa * programaAImprimir);
bool imprimirListaDeProgramas();
#endif
