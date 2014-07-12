#include "pcp.h"
#include "colas.h"
#include "config.h"
#include "io.h"
#include "systemcalls.h"

t_log *logpcp;

extern uint32_t multiprogramacion;
extern pthread_mutex_t multiprogramacionMutex;

pthread_t dispatcherThread;
extern sem_t semKernel;
sem_t dispatcherReady, dispatcherCpu;

void *IniciarPcp(void *arg)
{
	//logpcp = log_create("log_pcp.txt", "KernelPCP", 1, LOG_LEVEL_TRACE);
	log_debug(logpcp, "Thread iniciado");

	pthread_create(&dispatcherThread, NULL, &Dispatcher, NULL);

	iniciarServidorCpu();

	pthread_join(&dispatcherThread, NULL);

	log_debug(logpcp, "Thread concluido");
	log_destroy(logpcp);

	sem_post(&semKernel);
	return NULL;
}

bool iniciarServidorCpu()
{
	bool nuevoMensaje(int socketCPU) {
		if ( recibirYprocesarPedido(socketCPU) == false ) {
			desconexionCPU(socketCPU);
			return false;
		}
		log_info(logpcp, "MP: %d, New: %d, Ready: %d, Exec: %d, Block, %d, Exit: %d, CPUready: %d, CPUexec: %d",multiprogramacion,queue_size(newQueue),queue_size(readyQueue),queue_size(execQueue),queue_size(blockQueue),queue_size(exitQueue),queue_size(cpuReadyQueue),queue_size(cpuExecQueue));
		return true;
	}

	log_debug(logpcp, "Iniciando servidor de CPUs");

	if(crearServidorNoBloqueante(config_get_int_value(config, "PUERTO_CPU"), nuevoMensaje, logpcp) == -1) {
		log_error(logpcp, "Hubo un problema en el servidor receptor de CPUs");
	}

	return true;
}

/*
 * Para activar este hilo usar las siguientes instrucciones
 * sem_post(&dispatcherReady);
 * sem_post(&dispatcherCpu);
 */
void *Dispatcher(void *arg)
{
	log_debug(logpcp, "Dispatcher Thread iniciado");

	sem_init(&dispatcherReady, 0, 0);
	sem_init(&dispatcherCpu, 0, 0);

	while(1)
	{
		log_debug(logpcp, "Dispatcher esperando");
		MoverReadyAExec();
		log_debug(logpcp, "Dispatcher completo operacion");
	}

	sem_destroy(&dispatcherReady);
	sem_destroy(&dispatcherCpu);

	log_debug(logpcp, "Dispatcher Thread concluido");

	return NULL;
}

void MoverReadyAExec()
{
	pcb_t *pcb = NULL;
	cpu_info_t *cpuInfo = NULL;

	do
	{
		sem_wait(&dispatcherReady);

		pthread_mutex_lock(&readyQueueMutex);
		if( queue_is_empty(readyQueue) )
			log_error(logpcp, "Se llamo al dispatcher sin tener procesos en READY");
		else
			pcb = queue_peek(readyQueue);
		pthread_mutex_unlock(&readyQueueMutex);
	} while(pcb == NULL);

	//CHECK
	if( buscarProgramaConectado(pcb->id) == NULL )
	{
		moverAExit(sacarDeReady(pcb->id));
		return;
	}

	do
	{
		sem_wait(&dispatcherCpu);
		pthread_mutex_lock(&cpuReadyQueueMutex);
		if( queue_is_empty(cpuReadyQueue) )
			log_error(logpcp, "Se llamo al dispatcher sin tener una CPU disponible");
		else
			cpuInfo = queue_pop(cpuReadyQueue);
		pthread_mutex_unlock(&cpuReadyQueueMutex);
	} while(cpuInfo == NULL);


	log_info(logpcp, "Moviendo PCB de la cola READY a EXEC");

	moverAExec(sacarDeReady(pcb->id));

	cpuInfo->pid = pcb->id;

	//Mandando informacion necesaria para que la CPU pueda empezar a trabajar
	socket_pcb spcb;

	spcb.header.code = 'p';
	spcb.header.size = sizeof(socket_pcb);
	spcb.pcb = *pcb;

	moverCpuAExec(cpuInfo);

	if( send(cpuInfo->socketCPU, &spcb, sizeof(socket_pcb), 0) <= 0 )
		desconexionCPU(cpuInfo->socketCPU);
}

bool conexionCPU(int socketCPU)
{
	log_info(logpcp, "Se ha conectado un CPU");

	socket_header header;

	if( recv(socketCPU, &header, sizeof(socket_header), MSG_WAITALL) != sizeof(socket_header) )
		return false;

	//Agregandolo a la cpuReadyQueue
	cpu_info_t *cpuInfo = malloc(sizeof(cpu_info_t));
	cpuInfo->socketCPU = socketCPU;

	moverCpuAReady(cpuInfo);

	//Hay que mandar QUANTUM y RETARDO al CPU
	socket_cpucfg cpucfg;

	cpucfg.header.code = 'c';
	cpucfg.header.size = sizeof(socket_cpucfg);

	cpucfg.quantum = config_get_int_value(config, "QUANTUM");
	cpucfg.retardo = config_get_int_value(config, "RETARDO");

	if( send(socketCPU, &cpucfg, sizeof(socket_cpucfg), 0) <= 0 )
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

void desconexionCPU(int socketCPU)
{
	log_info(logpcp, "Se ha desconectado un CPU");
	log_info(logpcp, "Verificando si el CPU desconectado estaba corriendo algun programa");

	cpu_info_t *cpuInfo = sacarCpuDeExec(socketCPU);

	if( cpuInfo != NULL )
	{
		log_error(logpcp, "La CPU desconectada estaba en ejecucion, abortando ejecucion de programa");
		log_info(logpcp, "Moviendo PCB de la cola EXEC a EXIT");

		pcb_t *pcb = sacarDeExec(cpuInfo->pid);
		moverAExit(pcb);

		log_info(logpcp, "Informandole a Programa que el script no pudo concluir su ejecucion");

		mensajeYDesconexionPrograma(pcb->programaSocket, "El script no pudo concluir su ejecucion debido a la desconexion de un CPU");

		free(cpuInfo);
	}
	else
	{
		log_info(logpcp, "La CPU desconectada no se encontraba en ejecucion");
		cpuInfo = sacarCpuDeReady(socketCPU);

		if( cpuInfo != NULL )
			free(cpuInfo);
	}
}

bool recibirYprocesarPedido(int socketCPU)
{
	socket_header header;
	if( recv(socketCPU, &header, sizeof(header), MSG_WAITALL | MSG_PEEK) != sizeof(socket_header) )
		return false;

	switch(header.code)
	{
	case 'h': //Conectado
		return conexionCPU(socketCPU);
	case 'i': //SC: IO
		return syscallIO(socketCPU);
	case 'o': //SC: Obtener valor
		return syscallObtenerValor(socketCPU);
	case 'g': //SC: Grabar valor
		return syscallGrabarValor(socketCPU);
	case 'w': //SC: Wait
		return syscallWait(socketCPU);
	case 's': //SC: Signal
		return syscallSignal(socketCPU);
	case 'p': //Termino Quantum
		return terminoQuantumCPU(socketCPU);
	case 'k': //SC: Imprimir Texto
		return syscallImprimirTexto(socketCPU);
	default:
		log_error(logpcp, "Pedido invalido del cpu");
		return false;

	}
	return true;
}

bool terminoQuantumCPU(int socketCPU)
{
	socket_pcb spcb;

	if( recv(socketCPU, &spcb, sizeof(socket_pcb), MSG_WAITALL) != sizeof(socket_pcb) )
		return false;

	log_debug(logpcp, "CPU: %d, termino quantum", socketCPU);

	cpu_info_t *cpuInfo = sacarCpuDeExec(socketCPU);

	//CHECK
	if(cpuInfo == NULL)
	{
		log_error(logpcp, "La CPU que quiere devolver una PCB no se encontraba en ejecucion");
		return false;
	}

	if(spcb.terminoCpu)
		free(cpuInfo);
	else{
		moverCpuAReady(cpuInfo);
		sem_post(&dispatcherCpu);
	}

	pcb_t *pcb = sacarDeExec(spcb.pcb.id);

	//CHECK
	if(pcb == NULL)
	{
		log_error(logpcp, "La CPU devuelve una PCB de un proceso que no se encontraba en ejecucion");
		return false;
	}

	*pcb = spcb.pcb;

	conectados_t *conectado;
	log_debug(logpcp, "Codigo de error %d", pcb->lastErrorCode);
	switch(pcb->lastErrorCode)
	{
		case 0 : //Termino bien el quantum
			log_trace(logpcp, "Termino bien el quantum");

			if( buscarProgramaConectado(spcb.pcb.id) == NULL ){
				moverAExit(pcb);
				break;
			}

			moverAReady(pcb);
			sem_post(&dispatcherReady);
			break;

		case 1: //El programa finalizo correctamente
			log_trace(logpcp, "El programa finalizo correctamente");
			moverAExit(pcb);
			conectado = removerProgramaConectadoId(pcb->id);

			if(conectado != NULL){
				bajarNivelMultiprogramacion();
				free(conectado);
			}

			shutdown(pcb->programaSocket, SHUT_RDWR);
			break;

		case 2: //Segmentation fault
			log_trace(logpcp, "Segmentation fault");
			moverAExit(pcb);
			conectado = removerProgramaConectadoId(pcb->id);

			if(conectado != NULL){
				bajarNivelMultiprogramacion();
				free(conectado);
			}

			mensajeYDesconexionPrograma(pcb->programaSocket, "Segmentation fault");
			break;

		case 3: //Se solicito la posicion de memoria inexistente
			log_trace(logpcp, "Se solicito la posicion de memoria inexistente");
			moverAExit(pcb);
			conectado = removerProgramaConectadoId(pcb->id);

			if(conectado != NULL){
				bajarNivelMultiprogramacion();
				free(conectado);
			}

			mensajeYDesconexionPrograma(pcb->programaSocket, "Se solicito la posicion de memoria inexistente");
			break;

		case 4: //UMV error
			log_trace(logpcp, "UMV error");
			moverAExit(pcb);
			conectado = removerProgramaConectadoId(pcb->id);

			if(conectado != NULL){
				bajarNivelMultiprogramacion();
				free(conectado);
			}

			mensajeYDesconexionPrograma(pcb->programaSocket, "UMV error");
			break;

		case 5: //Label error
			log_trace(logpcp, "Label error");
			moverAExit(pcb);
			conectado = removerProgramaConectadoId(pcb->id);

			if(conectado != NULL){
				bajarNivelMultiprogramacion();
				free(conectado);
			}

			mensajeYDesconexionPrograma(pcb->programaSocket, "Label error");
			break;

		case 6:
			log_trace(logpcp, "Primitiva error");
			moverAExit(pcb);
			conectado = removerProgramaConectadoId(pcb->id);

			if(conectado != NULL){
				bajarNivelMultiprogramacion();
				free(conectado);
			}

			mensajeYDesconexionPrograma(pcb->programaSocket, "Primitiva error");
			break;
	}

	return true;
}
