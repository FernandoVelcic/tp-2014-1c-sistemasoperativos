#include "Consola.h"
#include "Segmento.h"
#include "Programa.h"
#include "stdio.h"
#include "stdlib.h"
#include "memoria.h"
#include "config.h"

extern t_log * logger;
extern pthread_rwlock_t lockEscrituraLectura;
extern t_list * tabla_segmentos;

char comandosBuffer[200];
FILE *archivoDump;


void * iniciarConsola(void * params) {

	printf("Consola de UMV, escriba-su-nombre-aqui\n");
	printf("All rights reserved. 2014\n\n");
	while (1) {

		char comando;
		printf("¿Que operacion desea realizar?\n");
		printf("Ingrese por teclado la letra de la operacion a realizar\n");
		printf("a - Operaciones con segmentos\n");
		printf("b - Modificar retardo ante una solicitud\n");
		printf("c - Modificar algoritmo de modificacion de segmentos\n");
		printf("d - Compactar tabla de segmentos\n");
		printf("e - Generar dump\n");

		comando = getchar();
		while (getchar() != '\n')
			;

		switch (comando) {
		case 'a':
			operacionesConSegmentos();
			break;
		case 'b':
			modificarRetardoUMV();
			break;
		case 'c':
			modificarAlgoCreacionSegmentos();
			break;
		case 'd':
			compactar();
			break;
		case 'e':
			archivoDump = fopen("dump.txt", "a");
			generarDump();
			fclose(archivoDump);
			break;
		default:
			log_error(logger, "El comando ingresado no es valido");
			printf("El comando ingresado es invalido\n");
			break;
		}
	}
	return NULL ;
}

void operacionesConSegmentos() {

	char operacion;

	printf("¿Que operacion desea realizar?\n");
	printf("a - Solicitar una posicion de memoria\n");
	printf("b - Escribir una posicion de memoria\n");
	printf("c - Crear un Segmento\n");
	printf("d - Destruir un Segmento\n");

	operacion = getchar();
	while (getchar() != '\n')
		;

	switch (operacion) {

	case 'a':
		requisitosOperacionSegmento(operacion);
		break;
	case 'b':
		requisitosOperacionSegmento(operacion);
		break;
	case 'c':
		printf("Ingrese el tamanio del segmento a crear\n");
		uint32_t tamanio;
		scanf("%d", &tamanio);
		while (getchar() != '\n')
			;
		if(tamanio > memoriaLibre()){
			printf("El tamaño solicitado excede el espacio libre en memoria");
		}else{
			crearSegmento(tamanio);

		}

		break;

	case 'd':
		printSegmentos( tabla_segmentos, PorCONSOLA);
		printf("Ingrese el ID del segmento a destruir");
		uint32_t idSeg;

		scanf("%d", &idSeg);
		while (getchar() != '\n');
		pthread_rwlock_wrlock(&lockEscrituraLectura);
		Segmento * segmento = buscarSegmentoEnTabla( idSeg);
		borrarSegmento( segmento);

		pthread_rwlock_unlock(&lockEscrituraLectura);
		break;

	default:
		printf("El comando ingresado es invalido\n");
	}

}

void requisitosOperacionSegmento(char operacion) {

	usleep( retardoUMV * 1000);
	printSegmentos( tabla_segmentos, PorCONSOLA);

	pthread_rwlock_wrlock(&lockEscrituraLectura);


	printf("Ingrese la base del segmento a solicitar\n");
	uint32_t base;
	scanf("%d", &base);
	while (getchar() != '\n')
		;

	printf("Ingrese un offset al segmento\n");
	uint32_t offset;
	scanf("%d", &offset);
	while (getchar() != '\n')
		;

	printf("Ingrese el tamanio que desea solicitar\n");
	uint32_t tamanio;
	scanf("%d", &tamanio);
	while (getchar() != '\n')
		;

		switch (operacion) {
		case 'a':solicitarPosicion(base, offset, tamanio);
				break;

		case 'b': escribirPosicion(base, offset, tamanio);
				break;

		default: break;
		}

	pthread_rwlock_unlock(&lockEscrituraLectura);
}


void solicitarPosicion( uint32_t base, uint32_t offset,	uint32_t tamanio) {

	uint32_t solicitud = solicitarPosicionDeMemoria( base, offset,	tamanio);

	if (solicitud == -1)
		printf("No se ha podido realizar la lectura de memoria\n");

}

void escribirPosicion( uint32_t base, uint32_t offset,	uint32_t tamanio) {

	printf("Ingrese el buffer que desea escribir, presionando ENTER entre cada byte\n");
	uint32_t buffer[tamanio];

	uint32_t i;
	for (i = 0; i < tamanio; i++) {
		scanf("%d", &buffer[i]);
		while (getchar() != '\n')
			;
	}

	uint32_t escritura = escribirPosicionDeMemoria( base, offset,
			tamanio, buffer);

	if (escritura == -1)
		printf("No se ha podido realizar la escritura de memoria\n");
}

void modificarRetardoUMV() {

	printf("Ingrese el retardo que quiere modificar en milisegundos \n");
	scanf("%d", &retardoUMV);
	while (getchar() != '\n')
		;
	printf("Este es el nuevo retardo %d\n", retardoUMV);
}

void modificarAlgoCreacionSegmentos() {

	printf("El algoritmo actual de creacion de segmentos es ");
	if( modoActualCreacionSegmentos == WORSTFIT){
		printf("WORST-FIT\n");
	} else printf("FIRST-FIT\n");

	printf("Desea cambiarlo? \ns- Si \nn- No\n");
	char cambio;
	cambio = getchar();
	while(getchar()!='\n');
	if( cambio == 's'){
		if( modoActualCreacionSegmentos == WORSTFIT){
			modoActualCreacionSegmentos = FIRSTFIT;
		} else modoActualCreacionSegmentos = WORSTFIT;
	}

	printf("El algoritmo actual de creacion de segmentos es ");
	if( modoActualCreacionSegmentos == WORSTFIT){
		printf("WORS-FIT\n");
	} else printf("FIRST-FIT\n");

}

void generarDump() {
	char reporte;

	printf("Indique que reporte quiere\n");
	printf("a-Tabla de segmentos de todos los procesos\n");
	printf("b-Tabla de segmento de un proceso en particular\n");
	printf("c-Detalle de la memoria principal\n");
	printf("d-Contenido de la memoria principal\n");

	reporte = getchar();
	while (getchar() != '\n')
		;

	switch (reporte) {
	case 'a':
		fprintf( archivoDump, "\nTabla de segmentos de todos los procesos:\n");
		printSegmentosPorPrograma();
		break;
	case 'b':
		buscarProgramaEImprimirSegmentos();
		break;
	case 'c':
		fprintf( archivoDump, "\nDetalle de la memoria principal;\n");
		printSegmentos(tabla_segmentos, PorARCHIVO);
		break;
	case 'd':
		fprintf(archivoDump, "\nContenido de la memoria principal:\n");
		pthread_rwlock_rdlock(&lockEscrituraLectura);
		imprimirMemoria();
		pthread_rwlock_unlock(&lockEscrituraLectura);
		break;
	default:
		log_error(logger, "El comando ingresado es invalido");
		//Esto lo imprimo en consola directamente
		printf("el comando ingresado es invalido\n");
	}
}

void imprimirMemoria(){
	uint32_t offset, tamanio;
	printf("Indique un offset desde el comienzo de la memoria\n");
	scanf("%d", &offset);
	while( getchar() != '\n');
	printf("Indique una cantidad de bytes\n");
	scanf("%d", &tamanio);
	while( getchar() != '\n');
	printf("El tamanio es: %d\n", tamanio);
	printf("El offset es: %d\n", offset);

	if( memoria_size < (tamanio + offset)){
		log_error( logger, "Se ha producido Segmentation Fault a causa de indicar una cantidad de bytes que sobrepasa el tamaño de la memoria");
		printf("Se ha producido Segmentation Fault a causa de indicar una cantidad de bytes que sobrepasa el tamaño de la memoria\n");
	}
	else{
		imprimirBytes( 0, offset, tamanio, PorARCHIVO);
	}
}

void printSegmentosHeaders(char porDondeImprimo) {
	if (porDondeImprimo == PorCONSOLA){
		printf( "\n\t\t|  Inicio Real  |   Fin  Real   |     Tamaño\n");
		printf( "----------------|---------------|---------------|-------------|\n");
	}
	else{
		fprintf( archivoDump, "\n\t\t\t|  Inicio Real  \t|     Fin  Real   \t|  Tamaño\t|\n");
		fprintf( archivoDump, "-------------------|-------------------|------------------------|-------------------|\n");
	}
}

void imprimirSegmentosDe(Programa *programa) {
	fprintf( archivoDump, "Programa: %d \n", programa->pid);
	printSegmentosHeaders(PorARCHIVO);
	printSegmento(programa->stack, PorARCHIVO);
	printSegmento(programa->script, PorARCHIVO);
	printSegmento(programa->etiquetas, PorARCHIVO);
	printSegmento(programa->instrucciones, PorARCHIVO);
}

void printSegmentosPorPrograma() {
	pthread_rwlock_rdlock(&lockEscrituraLectura);
	uint32_t tamanioProgramas;
	tamanioProgramas = list_size(programas);
	int i = 0;
	for (i = 0; i <= tamanioProgramas - 1; i++) {
		Programa * programa = list_get(programas, i);
		imprimirSegmentosDe(programa);
	}
	pthread_rwlock_unlock(&lockEscrituraLectura);

}

void buscarProgramaEImprimirSegmentos() {
	uint32_t pid;
	Programa *programaAImprimir;
	pthread_rwlock_rdlock(&lockEscrituraLectura);
	if(!imprimirListaDeProgramas()) return;
	printf("Ingrese el id del programa para imprimir sus segmentos\n");

	scanf("%d", &pid);

	programaAImprimir = buscarPrograma(pid);
	fprintf( archivoDump, "\nTabla de segmento de:");
	imprimirSegmentosDe(programaAImprimir);
	pthread_rwlock_unlock(&lockEscrituraLectura);
}

void printSegmentos(t_list * segmentos, char porDondeImprimo) {
	pthread_rwlock_rdlock(&lockEscrituraLectura);
	ordenarTablaSegmentos();
	printSegmentosHeaders(porDondeImprimo);

	void imprimirSeparador(char porDondeImprimo){
		if(porDondeImprimo == PorCONSOLA)
			printf("------------------|-----------------------|-----------------------|-----------------|\n");
		else
			fprintf( archivoDump, "-------------------|-------------------|------------------------|-------------------|\n");
	}

	int i = 0;
	uint32_t cont = 0;
	for (i = 0; i < list_size(segmentos); i++) {
		Segmento * segmento = (Segmento *) list_get(segmentos, i);
		if (segmento->inicioReal != cont) {
			printEspacioLibre(cont, segmento->inicioReal - 1, porDondeImprimo);
			cont = segmento->finReal + 1;


			imprimirSeparador(porDondeImprimo);

		}
		else{

		printSegmento(segmento, porDondeImprimo);
		cont = segmento->finReal + 1;
		}

		imprimirSeparador(porDondeImprimo);

	}
	if (cont < (memoria_size - 1)){
		printEspacioLibre(cont, (memoria_size - 1), porDondeImprimo);

		if (porDondeImprimo == PorCONSOLA)
			printf( "------------------------------------------------------------------------------------|\n");
		else
			fprintf( archivoDump, "------------------------------------------------------------------------------------|\n");

	}
	pthread_rwlock_unlock(&lockEscrituraLectura);
}

void printSegmento(Segmento * segmento, char porDondeImprimo) {
	if (porDondeImprimo == PorCONSOLA)
		printf( "\t%04d\t|\t%04d\t|\t%04d\t|\t%05d |\n", segmento->id, segmento->inicioReal, segmento->finReal, tamanioSegmento(segmento));
	else
		fprintf( archivoDump, "\t%04d\t|\t  %04d\t|\t    %04d\t\t|\t%05d\t|\n", segmento->id, segmento->inicioReal, segmento->finReal, tamanioSegmento(segmento));

}

void printEspacioLibre(uint32_t inicioEspacio, uint32_t finEspacio, char porDondeImprimo) {
	if (porDondeImprimo == PorCONSOLA)
		printf("\tLibre\t|\t%04d\t|\t%04d\t|\t%05d\n", inicioEspacio, finEspacio, finEspacio - inicioEspacio + 1);
	else
		fprintf( archivoDump, "\tLibre\t|\t  %04d  \t|\t   %04d  \t|\t%05d\t|\n", inicioEspacio, finEspacio, finEspacio - inicioEspacio + 1);
}

int imprimirListaDeProgramas(){
	int i;
	Programa *programa;
	if (!list_is_empty(programas)){
		printf("\n\t| Id Programa \t|\n");
		for (i = 0;i<list_size(programas); i++){
			programa = list_get(programas, i);
			printf("\t| %d \t\t|\n", programa->pid);

		}
		return 1;
	}
	printf("\nNo hay programas en memoria\n\n");
	return 0;

}
