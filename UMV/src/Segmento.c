#include "Segmento.h"
#include "config.h"

extern t_log *logger;

extern pthread_rwlock_t lockEscrituraLectura;

Segmento * new_Segmento( uint32_t inicio, uint32_t fin )
{
	Segmento *segmento		= malloc(sizeof(Segmento));
	segmento->inicioReal	= inicio;
	segmento->finReal		= fin - 1;

	segmento->finVirtual	= 0;
	segmento->inicioVirtual = 0;

	return segmento;

}

bool memCopy(Segmento *segmento, uint32_t offset, void *valor, uint32_t length)
{
	int32_t tamanioParaOperar = tamanioSegmento(segmento) - offset;

	if((int32_t) length > tamanioParaOperar)
	{
		log_error(logger, "Segmentation fault al copiar: length: %d, tamanioParaOperar: %d, base: %d, offset: %d", length, tamanioParaOperar, segmento->inicioVirtual, offset);
		return false;
	}

	pthread_rwlock_rdlock(&lockEscrituraLectura);
	memcpy(memoria + segmento->inicioReal + offset, valor, length);
	pthread_rwlock_unlock(&lockEscrituraLectura);

	log_info(logger, "Se guardo la data: %s", valor);

	return true;
}

bool memLeer(Segmento *segmento, void *destino, uint32_t offset, uint32_t length)
{
	int32_t tamanioParaOperar = tamanioSegmento(segmento) - offset;

	if((int32_t) length > tamanioParaOperar)
	{
		log_error(logger, "Segmentation fault al leer: length: %d, tamanioParaOperar: %d, base: %d, offset: %d", length, tamanioParaOperar, segmento->inicioVirtual, offset);
		return false;
	}

	pthread_rwlock_rdlock(&lockEscrituraLectura);
	memcpy(destino, memoria + segmento->inicioReal + offset, length);
	pthread_rwlock_unlock(&lockEscrituraLectura);

	log_debug(logger, "Se leyo la data: %s", destino);

	return true;
}

