#include "Programa.h"
#include "memoria.h"
#include "config.h"

extern t_log * logger;
extern pthread_rwlock_t lockEscrituraLectura;

Programa *crearPrograma(uint32_t pid, void *script, void *etiquetas, void *instrucciones_serializado, socket_pedirMemoria *pedidoMemoria)
{
	Programa *programa = malloc(sizeof(Programa));
	programa->pid = pid;

	pthread_rwlock_rdlock(&lockEscrituraLectura);

	programa->stack = crearSegmento(pedidoMemoria->stackSegmentSize);
	programa->script = crearYllenarSegmento(pedidoMemoria->codeSegmentSize, script);
	programa->instrucciones = crearYllenarSegmento(pedidoMemoria->instruccionesSegmentSize,instrucciones_serializado);

	if(list_is_empty(programas))
	{
		crearDireccionesVirtuales(programa->stack, pedidoMemoria->stackSegmentSize, 0);
	}else{
		Programa *ultimoPrograma = list_get(programas, list_size(programas) - 1);
		crearDireccionesVirtuales(programa->stack, pedidoMemoria->stackSegmentSize, ultimoPrograma->instrucciones->finVirtual);
	}

	crearDireccionesVirtuales(programa->script, pedidoMemoria->codeSegmentSize, programa->stack->finVirtual);

	if(pedidoMemoria->etiquetasSegmentSize == 0)
	{
		programa->etiquetas = NULL;
		crearDireccionesVirtuales(programa->instrucciones, pedidoMemoria->instruccionesSegmentSize, programa->script->finVirtual);
	}else{
		programa->etiquetas = crearYllenarSegmento(pedidoMemoria->etiquetasSegmentSize, etiquetas);
		crearDireccionesVirtuales( programa->etiquetas, pedidoMemoria->etiquetasSegmentSize, programa->script->finVirtual);
		crearDireccionesVirtuales( programa->instrucciones, pedidoMemoria->instruccionesSegmentSize, programa->etiquetas->finVirtual);
	}

	list_add(programas, programa);

	pthread_rwlock_unlock(&lockEscrituraLectura);

	return programa;

}

socket_umvpcb crearEstructuraParaPCB(Programa * programa)
{
	socket_umvpcb datosSegmentos;
	uint32_t etiquetas;

	datosSegmentos.stackSegment = programa->stack->inicioVirtual;
	datosSegmentos.codeSegment = programa->script->inicioVirtual;
	if( programa->etiquetas == NULL)
	{
		etiquetas = SEGMENTOVACIO;
	}else{
		etiquetas = programa->etiquetas->inicioVirtual;
	}

	datosSegmentos.etiquetaIndex = etiquetas;
	datosSegmentos.codeIndex = programa->instrucciones->inicioVirtual;

	return datosSegmentos;
}

Segmento * crearDireccionesVirtuales(Segmento * segmento, uint32_t tamanioSegmento, uint32_t finVirtualDelAnterior)
{
	uint32_t seed = finVirtualDelAnterior + 1;
	segmento->inicioVirtual = rand() % 417 + seed;
	segmento->finVirtual = segmento->inicioVirtual + tamanioSegmento - 1;

	return segmento;
}


Programa *buscarPrograma(uint32_t pid)
{
	bool matchearPrograma(Programa *nodoPrograma)
	{
		return nodoPrograma->pid == pid;
	}

	return list_find(programas, matchearPrograma);
}

Segmento * buscarSegmentoEnProgramaPorVirtual(Programa * programa, uint32_t base)
{
	log_info( logger, "La base que estoy buscando es %d 2", base);

	if (base == programa->stack->inicioVirtual)
	{
		log_info( logger,"Es el stack!");
		return programa->stack;
	}
	if (base == programa->script->inicioVirtual)
	{
		log_info( logger,"Es el script!");
		return programa->script;
	}
	if(programa->etiquetas != NULL)
	{
		if (base == programa->etiquetas->inicioVirtual)
		{
			log_info( logger,"Es el de etiquetas!");
			return programa->etiquetas;
		}
	}
	if (base == programa->instrucciones->inicioVirtual)
	{
		log_info( logger,"Es el de instrucciones!");
		return programa->instrucciones;
	}

	return NULL ;
}

Segmento * buscarSegmentoEnProgramaPorReal(Programa * programa, uint32_t base)
{
	log_info( logger, "La base que estoy buscando es %d 2", base);

	if (base == programa->stack->inicioReal)
		return programa->stack;
	if (base == programa->script->inicioReal)
		return programa->script;
	if(programa->etiquetas != NULL)
	{
		if (base == programa->etiquetas->inicioReal)
			return programa->etiquetas;
	}

	if (base == programa->instrucciones->inicioReal)
		return programa->instrucciones;

	return NULL ;

}


bool destruirPrograma( Programa * programa )
{
	bool matchearPrograma(Programa *nodoPrograma)
	{
		return nodoPrograma->pid == programa->pid;
	}

		list_remove_by_condition( programas, matchearPrograma);

		if( programa != NULL)
		{
			log_info( logger, "Destruyendo programa con pid: %d", programa->pid);
			pthread_rwlock_rdlock(&lockEscrituraLectura);
			borrarSegmento( programa->stack );
			borrarSegmento( programa->script );

			if(programa->etiquetas != NULL)
			borrarSegmento( programa->etiquetas );

			borrarSegmento( programa->instrucciones );
			pthread_rwlock_unlock(&lockEscrituraLectura);
			free( programa );

			return true;
		}else{
			log_error( logger, "El programa con pid: %d no se ha podido destruir", programa->pid);
			return false;
		}
}

