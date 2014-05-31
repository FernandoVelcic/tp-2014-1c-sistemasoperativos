#include "plp.h"
#include "colas.h"
#include "config.h"

#define CALCULAR_PRIORIDAD(e,f,t) (5 * e + 3 * f + t)

#define UMV_ENABLE

t_log *logplp;

uint32_t nextProcessId = 1;
uint32_t multiprogramacion = 0;
pthread_mutex_t multiprogramacionMutex = PTHREAD_MUTEX_INITIALIZER;

extern sem_t dispatcherReady;

int socketUMV;


void *IniciarPlp(void *arg) {
	logplp = log_create("log_plp.txt", "KernelPLP", 1, LOG_LEVEL_TRACE);
	log_info(logplp, "Thread iniciado");

#ifdef UMV_ENABLE
	socketUMV = conectar(config_get_string_value(config, "IP_UMV"), config_get_int_value(config, "PUERTO_UMV"), logplp);

	if (socketUMV == -1) {
		log_error(logplp, "No se pudo establecer la conexion con la UMV");
		log_info(logplp, "Thread concluido");
		log_destroy(logplp);
		return NULL ;
	}

	log_info(logplp, "Conectado con la UMV");
#endif

	if (crearServidorNoBloqueante(config_get_int_value(config, "PUERTO_PROG"), nuevoMensaje, logplp) == -1) {
		log_error(logplp, "No se pudo crear el servidor receptor de Programas");
	}

	log_info(logplp, "Thread concluido");
	log_destroy(logplp);
	return NULL ;
}

void MoverNewAReady()
{
	log_info(logplp, "Ordenando la cola NEW segun algoritmo de planificacion SJN");

	bool sjnAlgorithm(pcb_t *a, pcb_t *b)
	{
		return a->prioridad < b->prioridad;
	}
	list_sort(newQueue->elements, sjnAlgorithm);

	log_info(logplp, "Moviendo PCB de la cola NEW a READY");
	pthread_mutex_lock(&readyQueueMutex);
	pthread_mutex_lock(&multiprogramacionMutex);
	queue_push(readyQueue, queue_pop(newQueue));
	multiprogramacion++;
	pthread_mutex_unlock(&multiprogramacionMutex);
	pthread_mutex_unlock(&readyQueueMutex);

	//Llamada a dispatcher del PCP para avisar que hay un nuevo trabajo pendiente
	sem_post(&dispatcherReady);
}

void puedoMoverNewAReady()
{
	log_info(logplp, "Verificando grado de multiprogramacion");

	if(multiprogramacion < config_get_int_value(config, "MULTIPROGRAMACION") && !queue_is_empty(newQueue)) {
		MoverNewAReady();
	}
}

void desconexionCliente()
{
	log_info(logplp, "Se ha desconectado un Programa");
	puedoMoverNewAReady();
}


bool nuevoMensaje(int socket) {
	if (recibirYprocesarScript(socket) == false) {
		desconexionCliente();
		return false;
	}

	return true;
}

bool recibirYprocesarScript(int socket) {
	//Pidiendo script ansisop
	socket_header header;

	if( recv(socket, &header, sizeof(socket_header), MSG_WAITALL) != sizeof(socket_header) )
		return false;

	int scriptSize = header.size - sizeof(header);

	char *script = malloc(scriptSize + 1);
	memset(script, 0x00, scriptSize + 1);

	log_info(logplp, "Esperando a recibir un script ansisop");

	if( recv(socket, script, scriptSize, MSG_WAITALL) != scriptSize )
		return false;

	log_info(logplp, "Script ansisop recibido");

	//ansisop preprocesador
	t_metadata_program *scriptMetadata = metadata_desde_literal(script);

	log_info(logplp, "Pidiendole memoria a la UMV para que pueda correr el script ansisop");

	socket_pedirMemoria pedirMemoria;
	pedirMemoria.header.size = sizeof(pedirMemoria);

	pedirMemoria.codeSegmentSize = scriptSize + 1;
	pedirMemoria.stackSegmentSize = config_get_int_value(config, "STACK_SIZE");
	pedirMemoria.etiquetasSegmentSize = scriptMetadata->etiquetas_size;
	pedirMemoria.instruccionesSegmentSize = scriptMetadata->instrucciones_size * sizeof(t_intructions);

#ifdef UMV_ENABLE
	send(socketUMV, &pedirMemoria, sizeof(socket_pedirMemoria), 0);
#endif

	socket_respuesta respuesta;

	recv(socketUMV, &respuesta, sizeof(socket_respuesta), MSG_WAITALL);

	if(respuesta.valor == true)
	{
		log_info(logplp, "La UMV informo que pudo alojar la memoria necesaria para el script ansisop");
		log_info(logplp, "Enviando a la UMV los datos a guardar en los segmentos");

		send(socketUMV, &nextProcessId, sizeof(nextProcessId), 0);
		send(socketUMV, script, pedirMemoria.codeSegmentSize, 0);
		send(socketUMV, scriptMetadata->etiquetas, pedirMemoria.etiquetasSegmentSize, 0);
		send(socketUMV, scriptMetadata->instrucciones_serializado, pedirMemoria.instruccionesSegmentSize, 0);

		socket_umvpcb umvpcb;
		recv(socketUMV, &umvpcb, sizeof(socket_umvpcb), MSG_WAITALL);

		pcb_t *pcb = malloc(sizeof(pcb_t));

		pcb->id = nextProcessId; nextProcessId++;

		pcb->codeSegment = umvpcb.codeSegment;
		pcb->stackSegment = umvpcb.stackSegment;
		pcb->stackCursor = umvpcb.stackSegment;
		pcb->codeIndex = umvpcb.codeIndex;
		pcb->etiquetaIndex = umvpcb.etiquetaIndex;
		pcb->etiquetasSize = scriptMetadata->etiquetas_size;
		pcb->programCounter = scriptMetadata->instruccion_inicio;
		pcb->contextSize = 0;

		pcb->programaSocket = socket;
		pcb->prioridad = CALCULAR_PRIORIDAD(scriptMetadata->cantidad_de_etiquetas, scriptMetadata->cantidad_de_funciones, scriptMetadata->instrucciones_size);
		pcb->lastErrorCode = 0;

		queue_push(newQueue, pcb);
		log_info(logplp, "Segmentos cargados en la UMV y PCB generada en la cola NEW");

		puedoMoverNewAReady();
	} else {
		log_error(logplp, "La UMV informo que no pudo alojar la memoria necesaria para el script ansisop");
		log_info(logplp, "Informandole a Programa que el script no se puede procesar por el momento");

		socket_msg msg;
		strcpy(msg.msg, "No hay memoria suficiente en este momento para ejecutar este script. Intentelo mas tarde");
		send(socket, &msg, sizeof(socket_msg), 0);

		return false;
	}

	metadata_destruir(scriptMetadata);
	free(script);

	return true;
}

