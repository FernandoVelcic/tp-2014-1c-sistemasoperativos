#include "pcp.h"
#include "colas.h"
#include "config.h"
#include "io.h"

t_log *logpcp;

extern uint32_t multiprogramacion;
extern pthread_mutex_t multiprogramacionMutex;

sem_t dispatcherReady, dispatcherCpu;

void *IniciarPcp(void *arg)
{
	logpcp = log_create("log_pcp.txt", "KernelPCP", 1, LOG_LEVEL_TRACE);
	log_info(logpcp, "Thread iniciado");

	pthread_t dispatcherThread;
	pthread_create(&dispatcherThread, NULL, &Dispatcher, NULL);

	if(crearServidorNoBloqueante(config_get_int_value(config, "PUERTO_CPU"), nuevoMensajeCPU, logpcp) == -1) {
		log_error(logpcp, "No se pudo crear el servidor receptor de CPUs");
	}

	pthread_join(&dispatcherThread, NULL);

	log_info(logpcp, "Thread concluido");
	log_destroy(logpcp);
	return NULL;
}

/*
 * Para activar este hilo usar las siguientes instrucciones
 * sem_post(&dispatcherReady);
 * sem_post(&dispatcherCpu);
 */
void *Dispatcher(void *arg)
{
	log_info(logpcp, "Dispatcher Thread iniciado");

	sem_init(&dispatcherReady, 0, 0);
	sem_init(&dispatcherCpu, 0, 0);

	while(1)
	{
		sem_wait(&dispatcherReady);
		sem_wait(&dispatcherCpu);

		pthread_mutex_lock(&readyQueueMutex);

		log_info(logpcp, "Dispatcher invocado");

		/*
		 * Si hay un trabajo en READY y no hay CPU libre no hacer nada
		 * Si no hay trabajos en READY y hay una CPU libre no hacer nada
		 * Si hay un trabajo en READY y hay una CPU libre ponerla a trabajar
		 */
		if( !queue_is_empty(readyQueue) && !queue_is_empty(cpuReadyQueue) )
		{
			log_info(logpcp, "Moviendo PCB de la cola READY a EXEC");
			queue_push(execQueue, queue_pop(readyQueue));

			//Iniciando proceso de ejecucion
			pcb_t *pcb = queue_pop(readyQueue);

			cpu_info_t *cpuInfo = queue_pop(cpuReadyQueue);
			cpuInfo->socketPrograma = pcb->programaSocket;

			//Mandando informacion necesaria para que la CPU pueda empezar a trabajar
			socket_pcb spcb;

			spcb.header.code = 'p';
			spcb.header.size = sizeof(socket_pcb);
			spcb.pcb = *pcb;

			queue_push(cpuExecQueue, cpuInfo);

			if( send(cpuInfo->socketCPU, &spcb, sizeof(socket_pcb), 0) <= 0 )
				desconexionCPU(cpuInfo->socketCPU);
		}

		pthread_mutex_unlock(&readyQueueMutex);
	}

	sem_destroy(&dispatcherReady);
	sem_destroy(&dispatcherCpu);

	log_info(logpcp, "Dispatcher Thread concluido");

	return NULL;
}

bool conexionCPU(int socket)
{
	log_info(logpcp, "Se ha conectado un CPU");

	//Agregandolo a la cpuReadyQueue
	cpu_info_t *cpuInfo = malloc(sizeof(cpu_info_t));
	cpuInfo->socketCPU = socket;
	queue_push(cpuReadyQueue, cpuInfo);

	//Hay que mandar QUANTUM y RETARDO al CPU
	socket_cpucfg cpucfg;

	cpucfg.header.code = 'c';
	cpucfg.header.size = sizeof(socket_cpucfg);

	cpucfg.quantum = config_get_int_value(config, "QUANTUM");
	cpucfg.retardo = config_get_int_value(config, "RETARDO");

	if( send(socket, &cpucfg, sizeof(socket_cpucfg), 0) <= 0 )
		return false;

	//Llamanda a dispatcher para ver si hay algun trabajo pendiente para darle al CPU nuevo.
	sem_post(&dispatcherCpu);

	return true;
}

void bajarNivelMultiprogramacion()
{
	pthread_mutex_lock(&multiprogramacionMutex);
	multiprogramacion--;
	pthread_mutex_unlock(&multiprogramacionMutex);
}

void desconexionCPU(int socket)
{
	log_info(logpcp, "Se ha desconectado un CPU");

	bool limpiarCpu(cpu_info_t *cpuInfo)
	{
		return cpuInfo->socketCPU == socket;
	}

	log_info(logpcp, "Verificando si el CPU desconectado estaba corriendo algun programa");
	cpu_info_t *cpuInfo = list_remove_by_condition(cpuExecQueue->elements, limpiarCpu);
	if( cpuInfo != NULL )
	{
		log_error(logpcp, "La CPU desconectada estaba en ejecucion, abortando ejecucion de programa");
		//La cpu estaba en ejecucion, hay que mover la pcb de EXEC a EXIT
		bool limpiarPcb(pcb_t *pcb) {
			return pcb->programaSocket == cpuInfo->socketPrograma;
		}

		log_info(logpcp, "Moviendo PCB de la cola EXEC a EXIT");
		queue_push(exitQueue, list_remove_by_condition(execQueue->elements, limpiarPcb));

		log_info(logpcp, "Informandole a Programa que el script no pudo concluir su ejecucion");

		socket_msg msg;

		strcpy(msg.msg, "El script no pudo concluir su ejecucion debido a la desconexion de un CPU");
		send(cpuInfo->socketPrograma, &msg, sizeof(msg), 0);
		shutdown(cpuInfo->socketPrograma, SHUT_RDWR);

		free(cpuInfo);
		bajarNivelMultiprogramacion();
	}
	else
	{
		log_info(logpcp, "La CPU desconectada no se encontraba en ejecucion");
		list_remove_and_destroy_by_condition(cpuReadyQueue->elements, limpiarCpu, free);
	}
}


bool nuevoMensajeCPU(int socket) {

	if ( recibirYprocesarPedido(socket) == false ) {
		desconexionCPU(socket);
		return false;
	}

	return true;
}

bool recibirYprocesarPedido(int socket)
{
	socket_header header;
	if( recv(socket, &header, sizeof(header), MSG_WAITALL | MSG_PEEK) != sizeof(header) )
		return false;

	switch(header.code)
	{
	case 'h': //Conectado
		return conexionCPU(socket);
	case 'i': //SC: IO
		return syscallIO(socket);
	}
	return true;
}

bool syscallIO(int socket)
{
	socket_scIO io;
	socket_pcb spcb;

	if( recv(socket, &io, sizeof(socket_scIO), MSG_WAITALL) != sizeof(socket_scIO) )
		return false;

	if( recv(socket, &spcb, sizeof(socket_pcb), MSG_WAITALL) != sizeof(socket_pcb) )
		return false;

	io_t *disp = dictionary_get(dispositivos, io.identificador);
	data_cola_t *pedido = malloc(sizeof(data_cola_t));

	pedido->pid = spcb.pcb.id;
	pedido->tiempo = io.unidades;

	bool limpiarPcb(pcb_t *pcb) {
		return pcb->id == spcb.pcb.id;
	}

	pthread_mutex_lock(&blockQueueMutex);
	pthread_mutex_lock(&disp->mutex);
	queue_push(blockQueue, list_remove_by_condition(execQueue->elements, limpiarPcb));
	queue_push(disp->cola, pedido);
	pthread_mutex_unlock(&disp->mutex);
	pthread_mutex_unlock(&blockQueueMutex);

	sem_post(&disp->semaforo);

	return true;
}

bool syscallObtenerValor(int socket)
{
	socket_scObtenerValor sObtenerValor;

	if( recv(socket, &sObtenerValor, sizeof(socket_scObtenerValor), MSG_WAITALL) != sizeof(socket_scObtenerValor) )
		return false;

	uint32_t *valor = dictionary_get(variablesCompartidas,sObtenerValor->identificador);
	sObtenerValor->valor = *valor;

	if( send(socket,&sObtenerValor,sizeof(socket_scObtenerValor),0) != sizeof(socket_scObtenerValor) )
		return false;

	return true;
}

bool syscallGrabarValor(int socket)
{
	socket_scGrabarValor sGrabarValor;

	if( recv(socket, &sGrabarValor, sizeof(socket_scGrabarValor), MSG_WAITALL) != sizeof(socket_scGrabarValor) )
		return false;

	uint32_t *valor = dictionary_get(variablesCompartidas,sGrabarValor->identificador);
	*valor = sGrabarValor->valor;

	return true;
}
