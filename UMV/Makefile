OBJS +=  \
./Debug/config.o  \
./Debug/Consola.o  \
./Debug/CPU.o  \
./Debug/handshake.o  \
./Debug/Kernel.o  \
./Debug/memoria.o  \
./Debug/Programa.o  \
./Debug/Segmento.o  \
./Debug/UMV.o  \
../commons/Debug/bitarray.o  \
../commons/Debug/config.o  \
../commons/Debug/error.o  \
../commons/Debug/log.o  \
../commons/Debug/pcb.o  \
../commons/Debug/process.o  \
../commons/Debug/sockets.o  \
../commons/Debug/string.o  \
../commons/Debug/temporal.o  \
../commons/Debug/txt.o  \
../commons/Debug/collections/dictionary.o  \
../commons/Debug/collections/list.o  \
../commons/Debug/collections/queue.o  \
../commons/Debug/parser/metadata_program.o  \
../commons/Debug/parser/parser.o

all: crearDir config.o Consola.o CPU.o handshake.o Kernel.o memoria.o Programa.o Segmento.o UMV.o UMV

UMV: $(OBJS)
	gcc -lpthread -pthread -o ./Debug/UMV $(OBJS)

config.o: ./src/config.c ./src/config.h 
	gcc -I ../commons -c ./src/config.c -o ./Debug/config.o

Consola.o: ./src/Consola.c ./src/Consola.h 
	gcc -I ../commons -c ./src/Consola.c -o ./Debug/Consola.o

CPU.o: ./src/CPU.c ./src/CPU.h 
	gcc -I ../commons -c ./src/CPU.c -o ./Debug/CPU.o

handshake.o: ./src/handshake.c ./src/handshake.h 
	gcc -I ../commons -c ./src/handshake.c -o ./Debug/handshake.o

Kernel.o: ./src/Kernel.c ./src/Kernel.h 
	gcc -I ../commons -c ./src/Kernel.c -o ./Debug/Kernel.o

memoria.o: ./src/memoria.c ./src/memoria.h 
	gcc -I ../commons -c ./src/memoria.c -o ./Debug/memoria.o

Programa.o: ./src/Programa.c ./src/Programa.h 
	gcc -I ../commons -c ./src/Programa.c -o ./Debug/Programa.o

Segmento.o: ./src/Segmento.c ./src/Segmento.h 
	gcc -I ../commons -c ./src/Segmento.c -o ./Debug/Segmento.o

UMV.o: ./src/UMV.c 
	gcc -I ../commons -c ./src/UMV.c -o ./Debug/UMV.o

clear:
	rm ./Debug/*.o ./Debug/UMV

crearDir:
	mkdir -p Debug

