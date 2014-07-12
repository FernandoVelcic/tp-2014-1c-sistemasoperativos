#include "handshake.h"
#include "config.h"

extern t_log * logger;

void * handShake(void * socket)
{
	socket_header sHandshake;

	if( recv(*(int *) socket, &sHandshake, sizeof(socket_header), MSG_WAITALL) != sizeof(socket_header) )
	{
		log_error( logger, "No se ha recibido con exito quien trataba de conectarse");
		return NULL;
	}

	switch (sHandshake.code)
	{
	case 'k':
		fnKernelConectado(*(int*)socket);
		break;

	case 'c':
		fnNuevoCpu(*(int*)socket);
		break;

	default:
		log_error( logger, "El codigo enviado por quien trataba de conectarse es invalido");
	}

	return NULL ;
}

void * crearConexiones()
{
	crearServidor(config_get_int_value(umvConfig, "PUERTO"), handShake, logger);
	return NULL ;
}
