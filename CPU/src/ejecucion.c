#include "umv.h"

#include "commons/pcb.h"
#include "commons/parser/parser.h"


extern AnSISOP_funciones * ansisop_funciones;
extern t_log * logger;
extern pcb_t * PCB_enEjecucion;

//TODO no esta hecho todavia, es semi pseudocodigo
pcb_t * ejecutar () {

	printf("Ejecutando........\n\n");

	char a[] = "varibles a";
	char b[] = "a=5";
	char c[] = "print a";
	analizadorLinea( a, ansisop_funciones, NULL );
	//analizadorLinea( b, ansisop_funciones, NULL );
	//analizadorLinea( c, ansisop_funciones, NULL );

	//enviarCambioContexto( PCB_enEjecucion->id );

/*
	//while( 1 ){

		log_debug( logger, "Solicitando linea del programa a la umv" );
		char * instruccion = solicitarLineaPrograma( PCB_enEjecucion->programCounter );
		log_debug( logger, "Incrementando el program counter" );
		PCB_enEjecucion->programCounter++;
		log_debug( logger, "Ejecutando la intruccion !! " );
		analizadorLinea( instruccion, ansisop_funciones, NULL );

	//}

*/
	//enviarFinQuantum( PCB_enEjecucion->id );

	return PCB_enEjecucion;

}




