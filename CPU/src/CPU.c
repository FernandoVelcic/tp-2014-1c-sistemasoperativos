#include <stdio.h>
#include <stdbool.h>

#include "commons/log.h"
#include "commons/config.h"
#include "commons/pcb.h"

#include "config.h"
#include "umv.h"
#include "kernel.h"

#include "mocks.h"

t_log * logger;

uint32_t quantumPorEjecucion;
uint32_t retardo;
uint32_t quantumRestante;

pcb_t PCB_enEjecucion;


int main(int argc, char *argv[]) {
	if (argc != 2) {
		printf("Modo de empleo: ./CPU config.cfg\n");
		return EXIT_SUCCESS;
	}

	if( !cargar_config(argv[1]) ) {
		printf("Archivo de configuracion invalido\n");
		return EXIT_SUCCESS;
	}

	logger = log_create("log.txt", "CPU", 1, LOG_LEVEL_TRACE);
	log_info(logger, "Iniciando CPU");

	if ( !crearConexionKernel() || !crearConexionUMV() ) {
		log_error(logger, "Hubo un error en el proceso CPU, finalizando");
		close(socketKernel);
		close(socketUMV);
		return EXIT_SUCCESS;
	}

	ejecutarPrueba();
	//recibirYProcesarMensajesKernel();

	log_info(logger, "El programa finalizo con correctamente");

	log_destroy(logger);

	destruir_config();

	return EXIT_SUCCESS;

}

