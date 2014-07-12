#include "plp.h"
#include "colas.h"
#include "config.h"
#include "umv.h"

#define CALCULAR_PRIORIDAD(e,f,t) (5 * e + 3 * f + t)

t_log *logplp;

uint32_t nextProcessId = 1;
uint32_t multiprogramacion = 0;
pthread_mutex_t multiprogramacionMutex = PTHREAD_MUTEX_INITIALIZER;

extern sem_t semKernel;
extern sem_t dispatcherReady;


void *IniciarPlp(void *arg) {
	//logplp = log_create("log_plp.txt", "KernelPLP", 1, LOG_LEVEL_TRACE);
	log_debug(logplp, "Thread iniciado");

	if( conectarUMV() )
		iniciarServidorProgramas();

	log_debug(logplp, "Thread concluido");
	log_destroy(logplp);

	sem_post(&semKernel);
	return NULL;
}

bool iniciarServidorProgramas()
{
	bool nuevoMensaje(int socketPrograma) {
		if (recibirYprocesarScript(socketPrograma) == false) {
			desconexionCliente(socketPrograma);
			return false;
		}
		log_info(logplp, "MP: %d, New: %d, Ready: %d, Exec: %d, Block, %d, Exit: %d, CPUready: %d, CPUexec: %d",multiprogramacion,queue_size(newQueue),queue_size(readyQueue),queue_size(execQueue),queue_size(blockQueue),queue_size(exitQueue),queue_size(cpuReadyQueue),queue_size(cpuExecQueue));
		return true;
	}

	log_debug(logplp, "Iniciando servidor de Programas");

	if (crearServidorNoBloqueante(config_get_int_value(config, "PUERTO_PROG"), nuevoMensaje, logplp) == -1) {
		log_error(logplp, "Hubo un problema en el servidor receptor de Programas");
		return false;
	}

	return true;
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

	moverAReady(queue_pop(newQueue));

	pthread_mutex_lock(&multiprogramacionMutex);
	multiprogramacion++;
	pthread_mutex_unlock(&multiprogramacionMutex);

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

void desconexionCliente(int socketPrograma)
{
	conectados_t *conectado = removerProgramaConectadoPorSocket(socketPrograma);

	if(conectado != NULL){
		bajarNivelMultiprogramacion();
		free(conectado);
	}

	log_info(logplp, "Se ha desconectado un Programa");
	puedoMoverNewAReady();
}

bool recibirScriptAnsisop(int socketPrograma, char **script, uint32_t *scriptSize)
{
	socket_header header;

	if( recv(socketPrograma, &header, sizeof(socket_header), MSG_WAITALL) != sizeof(socket_header) )
		return false;

	*scriptSize = header.size - sizeof(socket_header);

	*script = malloc(*scriptSize + 1);
	memset(*script, 0x00, *scriptSize + 1);

	log_trace(logplp, "Esperando a recibir un script ansisop");

	if( recv(socketPrograma, *script, *scriptSize, MSG_WAITALL) != *scriptSize )
		return false;

	log_info(logplp, "Script ansisop recibido");

	return true;
}

pcb_t *crearPCB(int socketPrograma, socket_umvpcb *umvpcb, t_metadata_program *scriptMetadata)
{
	pcb_t *pcb = malloc(sizeof(pcb_t));

	pcb->id = nextProcessId; nextProcessId++;

	pcb->codeSegment = umvpcb->codeSegment;
	pcb->stackSegment = umvpcb->stackSegment;
	pcb->stackCursor = 0;
	pcb->codeIndex = umvpcb->codeIndex;
	pcb->etiquetaIndex = umvpcb->etiquetaIndex;
	pcb->etiquetasSize = scriptMetadata->etiquetas_size;
	pcb->programCounter = scriptMetadata->instruccion_inicio;
	pcb->contextSize = 0;

	pcb->programaSocket = socketPrograma;
	pcb->prioridad = CALCULAR_PRIORIDAD(scriptMetadata->cantidad_de_etiquetas, scriptMetadata->cantidad_de_funciones, scriptMetadata->instrucciones_size);
	pcb->lastErrorCode = 0;

	return pcb;
}

bool crearPrograma(int socketPrograma, char *script, uint32_t scriptSize, t_metadata_program *scriptMetadata)
{
	if( solicitarCreacionSegmentos(scriptSize, scriptMetadata) != true )
		return false;

	if( respuestaCreacionSegmentos() == true )
	{
		log_info(logplp, "La UMV informo que pudo alojar la memoria necesaria para el script ansisop");
		log_trace(logplp, "Enviando a la UMV los datos a guardar en los segmentos");

		if( enviarSegmentos(nextProcessId, script, scriptSize, scriptMetadata) != true )
			return false;

		socket_umvpcb umvpcb;

		if( respuestaSegmentos(&umvpcb) != true )
			return false;

		pcb_t *pcb = crearPCB(socketPrograma, &umvpcb, scriptMetadata);

		conectados_t *conectado = malloc(sizeof(conectados_t));
		conectado->pid = pcb->id;
		conectado->programaSocket = pcb->programaSocket;

		pthread_mutex_lock(&programasConectadosMutex);
		list_add(programasConectados, conectado);
		pthread_mutex_unlock(&programasConectadosMutex);

		queue_push(newQueue, pcb);
		log_info(logplp, "Segmentos cargados en la UMV y PCB generada en la cola NEW");

		puedoMoverNewAReady();
	} else {
		log_error(logplp, "La UMV informo que no pudo alojar la memoria necesaria para el script ansisop");
		mensajeYDesconexionPrograma(socketPrograma, "No hay memoria suficiente en este momento para ejecutar este script. Intentelo mas tarde");

		return false;
	}

	return true;
}

bool recibirYprocesarScript(int socketPrograma)
{
	char *script;
	uint32_t scriptSize;

	if( recibirScriptAnsisop(socketPrograma, &script, &scriptSize) != true )
		return false;

	//ansisop preprocesador
	t_metadata_program *scriptMetadata = metadata_desde_literal(script);

	bool result = crearPrograma(socketPrograma, script, scriptSize, scriptMetadata);

	metadata_destruir(scriptMetadata);
	free(script);

	return result;
}

void mensajeYDesconexionPrograma(int programaSocket, char *mensaje)
{
	socket_msg msg;
	msg.header.size = sizeof(socket_msg);
	msg.type = 1; //log_error

	strcpy(msg.msg, mensaje);
	send(programaSocket, &msg, sizeof(socket_msg), 0);

	log_trace(logplp, "Mensaje de error enviado al programa. Apagando socket: %d", programaSocket);

	shutdown(programaSocket, SHUT_RDWR);
}
