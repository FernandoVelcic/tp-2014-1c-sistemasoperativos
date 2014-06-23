#include "commons/collections/list.h"
#include "commons/log.h"
#include "commons/sockets.h"
#include "CPU.h"
#include "Programa.h"
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "memoria.h"
#include "Consola.h"

extern t_log * logger;

extern t_list * cpus;
extern uint32_t retardoUMV;
uint32_t contadorCpuId;
extern pthread_rwlock_t lockEscrituraLectura;



//En estos dos casos tiene que hacer los siguientes pasos
// 1. Buscar el programa que esta procesando
// 2. Obtener el segmento correspondiente al offset solicitado
// 3. Teniendo el socket delegarle la operacion
// 4. Responder dependiendo del estado

int procesarSolicitudLecturaMemoria( CPU * cpu, socket_leerMemoria * solicitud ) {

	char *buffer = malloc(solicitud->length+sizeof(socket_RespuestaLeerMemoria));
	log_trace(logger, "Solicitud de lectura, base = %d, offset = %d, length = %d", solicitud->base, solicitud->offset, solicitud->length);
	cpu->pidProcesando = solicitud->pdi;
	Programa * programa;
	programa = buscarPrograma( solicitud->pdi );
	pthread_rwlock_rdlock(&lockEscrituraLectura);
	Segmento * segmento;
	segmento = buscarSegmentoEnProgramaPorVirtual(programa, solicitud->base);


	if(segmento == NULL) {
		log_error(logger, "No se encuentra el segmento especificado | UMV/src/cpu.c -> procesarSolicitudLecturaMemoria");
		pthread_rwlock_unlock(&lockEscrituraLectura);
		return -1;
	}

	uint32_t tamanioParaOperar = tamanioSegmento(segmento) - solicitud->offset;

	if(solicitud->length > 10000){
		solicitud->length = 0;
		tamanioParaOperar = 0;
		log_error(logger, "Llego una solicitud de memoria demasiado larga");
	}

	uint32_t tamanioRespuesta = sizeof (socket_RespuestaLeerMemoria) + solicitud->length;

	socket_RespuestaLeerMemoria respuesta;
	respuesta.header.size = tamanioRespuesta;

	if(solicitud->length > tamanioParaOperar) {
		log_error(logger, "Segmentation fault, length: %d, tamanioParaOperar: %d, base: %d, offset: %d | UMV/src/cpu.c -> procesarSolicitudLecturaMemoria", solicitud->length, tamanioParaOperar, solicitud->base, solicitud->offset );
		respuesta.status = false;
	}else{
		respuesta.status = true;
		memLeer(segmento, buffer+sizeof(socket_RespuestaLeerMemoria), solicitud->offset, solicitud->length);
		log_debug(logger, "Se leyo la data: %s", buffer+sizeof(socket_RespuestaLeerMemoria));
	}

	memcpy(buffer, &respuesta, sizeof(socket_RespuestaLeerMemoria));

	uint32_t enviado = send(cpu->socket, buffer, tamanioRespuesta, 0);

	pthread_rwlock_unlock(&lockEscrituraLectura);
	free(buffer);
	if(enviado == -1) {
		log_error(logger, "Hubo un error al enviar la respuesta a lectura de memoria | UMV/src/cpu.c -> procesarSolicitudLecturaMemoria");
		return -1;
	}

	log_trace(logger, "Se pudo reponder a la cpu sin problemas, se enviaron %d bytes", enviado);

	//Si dejo esta linea cuando hay "segmentation fault" cierrar el socket de la cpu, además subi el free(respuesta)
	/*if(respuesta == false){
		return -1;
	}*/

	return 1;

}




int procesarSolicitudEscrituraMemoria( CPU * cpu, void *_solicitud ) {
	socket_guardarEnMemoria * solicitud = (socket_guardarEnMemoria *)_solicitud;
	char *buffer = malloc(solicitud->length);
	memcpy(buffer, _solicitud+sizeof(socket_guardarEnMemoria), solicitud->length);

	log_trace(logger, "Solicitud de escritura, base = %d, offset = %d, length = %d", solicitud->base, solicitud->offset, solicitud->length);

	cpu->pidProcesando = solicitud->pdi;
	Programa * programa;
	programa = buscarPrograma( solicitud->pdi);
	pthread_rwlock_rdlock(&lockEscrituraLectura);
	Segmento * segmento;
	segmento = buscarSegmentoEnProgramaPorVirtual(programa, solicitud->base);

	if(segmento == NULL) {
		log_error(logger, "No se encuentra el segmento especificado | UMV/src/cpu.c -> procesarSolicitudEscrituraMemoria");
		pthread_rwlock_unlock(&lockEscrituraLectura);
		return -1;
	}

	uint32_t tamanioParaOperar = tamanioSegmento(segmento) - solicitud->offset;
	socket_RespuestaGuardarEnMemoria * respuesta = malloc(sizeof (socket_RespuestaGuardarEnMemoria));
	respuesta->header.size = sizeof(socket_RespuestaGuardarEnMemoria);


	if(solicitud->length > tamanioParaOperar) {
		log_error(logger, "Segmentation fault, length: %d, tamanioParaOperar: %d, base: %d, offset: %d | UMV/src/cpu.c -> procesarSolicitudEscrituraMemoria", solicitud->length, tamanioParaOperar, solicitud->base, solicitud->offset );
		respuesta->status = false;
	}else{
		respuesta->status = true;
		memCopi(segmento, solicitud->offset, buffer, solicitud->length);
		log_info(logger, "Se guardo la data: %s", buffer);
	}

	uint32_t enviado = send( cpu->socket, respuesta, sizeof( socket_RespuestaGuardarEnMemoria ), 0 );

	pthread_rwlock_unlock(&lockEscrituraLectura);
	free(respuesta);

	if(enviado == -1) {
		log_error(logger, "La respuesta a escribir en memoria no se ha realizado con exito | UMV/src/cpu.c -> procesarSolicitudEscrituraMemoria");
		return -1;
	}

	log_info(logger, "La respuesta a escribir en memoria se ha realizado con exito");

	/*if(respuesta->status == false){
		free(respuesta);
		return -1;
	}*/

	free(buffer);

	return 1;
}




uint32_t recibirYProcesarMensajesCpu( CPU * cpu ) {

	int todoSaleBien = 1;

	while( todoSaleBien > 0 ){

		log_trace( logger, "Esperando un mensaje de la CPU..." );
		void * paquete = recibirPaquete( cpu->socket , 0, 'a', logger );

		if( paquete == NULL ){
			log_error( logger, "Se ha desconectado la CPU%d por causas desconocidas", cpu->cpuId);
			log_trace( logger, "Destruyendo programa activo de la CPU%d", cpu->cpuId);

			if( cpu->pidProcesando != SINPROCESOACTIVO){
			uint32_t programaDestruido;
			Programa * programa = buscarPrograma( cpu->pidProcesando);
			programaDestruido = destruirPrograma( programa);

			if( programaDestruido == true){
				log_info( logger, "El programa activo de la CPU%d se ha destruido correctamente", cpu->cpuId);
			}else
				log_error( logger, "El programa activo de la CPU%d no se ha destruido correctamente", cpu->cpuId);
			}
			return -1;
		}

		socket_header * header = ( socket_header * ) paquete;

		//log_debug(logger, "Esperando retardo para responder...");
		usleep(retardoUMV * 1000);

		switch ( header->code ){

			case 'b': todoSaleBien = procesarSolicitudLecturaMemoria( cpu, (socket_leerMemoria *) paquete ); break;
			case 'c': todoSaleBien = procesarSolicitudEscrituraMemoria( cpu, paquete ); break;

			case 'd':
				log_info( logger, "La CPU ID: %d informo que el programa activo con PID: %d ha finalizado", cpu->cpuId, cpu->pidProcesando);
				Programa * programa = buscarPrograma( cpu->pidProcesando);
				destruirPrograma( programa);
				cpu->pidProcesando = -1;
				break;

			default:

				log_error( logger, "La CPU ID: %d envio un codigo de mensaje invalido", cpu->cpuId );
				return -1;


		}

		free( paquete );

	}

	return todoSaleBien;

}

void borrarCPU( CPU * cpu ){
	bool matchearCPU( CPU * cpuAmatchear ){
		return cpuAmatchear->cpuId == cpu->cpuId;
	}
	list_remove_by_condition( cpus, matchearCPU);
	close(cpu->socket);
	free(cpu);
}




void fnNuevoCpu(int socket){

	log_info( logger, "Se conecto un nuevo CPU" );

	contadorCpuId++;
	CPU * cpu = malloc(sizeof(CPU));
	cpu->socket = socket;
	cpu->cpuId = contadorCpuId;
	cpu->pidProcesando = SINPROCESOACTIVO;
	list_add( cpus, cpu );


	if( recibirYProcesarMensajesCpu( cpu ) > 0 ){
		log_info( logger, "Se desconecto un cpu correctamente" );
	}else{
		log_error( logger, "Hubo un problema con el cpu ID %d: " , cpu->cpuId );
	}

	contadorCpuId--;
	log_info( logger, "Borrando  CPU%d", cpu->cpuId);
	borrarCPU( cpu);

}

