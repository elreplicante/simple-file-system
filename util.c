#include "util.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Formatea el disco virtual. Guarda el mapa de bits del super bloque 
// y el directorio único.

int myMkfs(MiSistemaDeFicheros* miSistemaDeFicheros, int tamDisco,
		char* nombreArchivo) {
	// Creamos el disco virtual:
	miSistemaDeFicheros->discoVirtual = open(nombreArchivo, O_CREAT | O_RDWR,
			S_IRUSR | S_IWUSR);

	int i;
	int numBloques = tamDisco / TAM_BLOQUE_BYTES;
	int minNumBloques = 3 + MAX_BLOQUES_CON_NODOSI + 1;
	int maxNumBloques = NUM_BITS;

	// Algunas comprobaciones mínimas:
	assert(sizeof (EstructuraSuperBloque) <= TAM_BLOQUE_BYTES);
	assert(sizeof (EstructuraDirectorio) <= TAM_BLOQUE_BYTES);

	if (numBloques < minNumBloques) {
		perror("Numero de bloques demasiado pequeño");
		return 1;
	}
	if (numBloques >= maxNumBloques) {
		perror("Numero de bloques demasiado grande");
		return 2;
	}

	/// MAPA DE BITS
	// Inicializamos el mapa de bits
	for (i = 0; i < NUM_BITS; i++) {
		miSistemaDeFicheros->mapaDeBits[i] = 0;
	}

	// Los primeros tres bloques tendrán el superbloque, mapa de bits y directorio
	miSistemaDeFicheros->mapaDeBits[SUPERBLOQUE_IDX] = 1;
	miSistemaDeFicheros->mapaDeBits[MAPA_BITS_IDX] = 1;
	miSistemaDeFicheros->mapaDeBits[DIRECTORIO_IDX] = 1;
	// Los siguientes NUM_INODE_BLOCKS contendrán nodos-i
	for (i = 3; i < 3 + MAX_BLOQUES_CON_NODOSI; i++) {
		miSistemaDeFicheros->mapaDeBits[i] = 1;
	}
	escribeMapaDeBits(miSistemaDeFicheros);

	/// DIRECTORIO
	// Inicializamos el directorio (numArchivos, archivos[i].libre) y lo escribimos en disco
	miSistemaDeFicheros->directorio.numArchivos = 0;
	int j;
	for (j = 0; j < MAX_ARCHIVOS_POR_DIRECTORIO; j++) {
		miSistemaDeFicheros->directorio.archivos[j].libre = 1;

	}
	escribeDirectorio(miSistemaDeFicheros);

	/// NODOS-I
	EstructuraNodoI nodoActual; //auxiliar para inicializacion
	nodoActual.libre = 1;
	// Escribimos nodoActual MAX_NODOSI veces en disco
	for (i = 0; i < MAX_NODOSI; i++) {
		miSistemaDeFicheros->nodosI[i] = &nodoActual;
		escribeNodoI(miSistemaDeFicheros, i, &nodoActual);
	}

	/// SUPERBLOQUE
	// Inicializamos el superbloque (ver common.c) y lo escribimos en disco
	initSuperBloque(miSistemaDeFicheros, tamDisco);
	escribeSuperBloque(miSistemaDeFicheros);
	sync();

	// Al finalizar tenemos al menos un bloque
	assert(myQuota(miSistemaDeFicheros) >= 1);

	printf("SF: %s, %d B (%d B/bloque), %d bloques\n", nombreArchivo, tamDisco,
			TAM_BLOQUE_BYTES, numBloques);
	printf("1 bloque para SUPERBLOQUE (%lu B)\n", sizeof(EstructuraSuperBloque));
	printf("1 bloque para MAPA DE BITS, que cubre %lu bloques, %lu B\n",
			NUM_BITS, NUM_BITS * TAM_BLOQUE_BYTES);
	printf("1 bloque para DIRECTORIO (%lu B)\n", sizeof(EstructuraDirectorio));
	printf("%d bloques para nodos-i (a %lu B/nodo-i, %lu nodos-i)\n",
			MAX_BLOQUES_CON_NODOSI, sizeof(EstructuraNodoI), MAX_NODOSI);
	printf("%d bloques para datos (%d B)\n",
			miSistemaDeFicheros->superBloque.numBloquesLibres, TAM_BLOQUE_BYTES
					* miSistemaDeFicheros->superBloque.numBloquesLibres);
	printf("¡Formato completado!\n");
	return 0;
}

int myImport(char* nombreArchivoExterno,
		MiSistemaDeFicheros* miSistemaDeFicheros, char* nombreArchivoInterno) {
	struct stat stStat;
	int handle = open(nombreArchivoExterno, O_RDONLY);
	if (handle == -1) {
		printf("Error, leyendo archivo %s\n", nombreArchivoExterno);
		return 1;
	}

	int nodoLibre = buscaNodoLibre(miSistemaDeFicheros);

	/// Comprobamos que podemos abrir el archivo a importar
	if (stat(nombreArchivoExterno, &stStat) != false) {
		perror("stat");
		fprintf(stderr, "Error, ejecutando stat en archivo %s\n",
				nombreArchivoExterno);
		return 2;
	}

	/// Comprobamos que hay suficiente espacio
	if (stStat.st_size >
			miSistemaDeFicheros->superBloque.numBloquesLibres * TAM_BLOQUE_BYTES){
		fprintf(stderr, "No hay suficiente espacio en disco\n");
		return 3;
	}

	/// Comprobamos que el tamaño total es suficientemente pequeño
	/// para ser almacenado en MAX_BLOCKS_PER_FILE
	if (stStat.st_size > (TAM_BLOQUE_BYTES * MAX_BLOQUES_POR_ARCHIVO)){
		fprintf(stderr, "El archivo a copiar es demasido grande\n");
		return 4;
	}

	/// Comprobamos que la longitud del nombre del archivo es adecuada
	if (strlen(nombreArchivoInterno) > MAX_TAM_NOMBRE_ARCHIVO){
		fprintf(stderr, "Nombre de archivo demasiado grande\n");
		return 5;
	}

	/// Comprobamos que el fichero no existe ya
	if (buscaPosDirectorio(miSistemaDeFicheros, nombreArchivoInterno) != -1){
		fprintf(stderr, "El archivo a copiar ya existe\n");
		return 6;
	}

	/// Comprobamos si existe un nodo-i libre
	if (nodoLibre == -1){
		fprintf(stderr, "No existen nodos-i libres\n");
		return 7;
	}

	/// Comprobamos que todavía cabe un archivo en el directorio (MAX_ARCHIVOS_POR_DIRECTORIO)
	if (miSistemaDeFicheros->directorio.numArchivos == MAX_ARCHIVOS_POR_DIRECTORIO){
		fprintf(stderr, "No caben mas archivos en el directorio\n");
		return 8;
	}

	/// Actualizamos toda la información:
	/// mapa de bits, directorio, nodo-i, bloques de datos, superbloque ...
	EstructuraNodoI *nodo = malloc(sizeof (EstructuraNodoI));
	nodo->tamArchivo = stStat.st_size;
	nodo->numBloques = (stStat.st_size / TAM_BLOQUE_BYTES) + 1;
	nodo->libre = 0;


	miSistemaDeFicheros->nodosI[nodoLibre] = nodo;
	reservaBloquesNodosI(miSistemaDeFicheros, nodo->idxBloques, nodo->numBloques);
	escribeNodoI(miSistemaDeFicheros, nodoLibre, nodo);
	escribeMapaDeBits(miSistemaDeFicheros);
	escribeDatos(miSistemaDeFicheros, handle, nodoLibre);
	escribeDirectorio(miSistemaDeFicheros);
	escribeSuperBloque(miSistemaDeFicheros);

	sync();
	close(handle);
	return 0;
}

int myExport(MiSistemaDeFicheros* miSistemaDeFicheros,
		char* nombreArchivoInterno, char* nombreArchivoExterno) {
	int handle;

	/// Buscamos el archivo nombreArchivoInterno en miSistemaDeFicheros
	// ...

	/// Si ya existe el archivo nombreArchivoExterno en linux preguntamos si sobreescribir
	// ...

	/// Copiamos bloque a bloque del archivo interno al externo
	// ...

	if (close(handle) == -1) {
		perror("myExport close");
		printf("Error, myExport close.\n");
		return 1;
	}
	return 0;
}

int myRm(MiSistemaDeFicheros* miSistemaDeFicheros, char* nombreArchivo) {
	/// Completar:
	// Busca el archivo con nombre "nombreArchivo"
	// Obtiene el nodo-i asociado y lo actualiza
	// Actualiza el superbloque (numBloquesLibres) y el mapa de bits
	// Libera el puntero y lo hace NULL
	// Actualiza el archivo
	// Finalmente, actualiza en disco el directorio, nodoi, mapa de bits y superbloque
	// ...
	return 0;
}

void myLs(MiSistemaDeFicheros* miSistemaDeFicheros) {
	//struct tm* localTime;
	int numArchivosEncontrados = 0;
	//EstructuraNodoI nodoActual;
	int i;
	// Recorre el sistema de ficheros, listando los archivos encontrados
	for (i = 0; i < MAX_ARCHIVOS_POR_DIRECTORIO; i++) {
		// ...
	}

	if (numArchivosEncontrados == 0) {
		printf("Directorio vacío\n");
	}
}

void myExit(MiSistemaDeFicheros* miSistemaDeFicheros) {
	int i;
	close(miSistemaDeFicheros->discoVirtual);
	for (i = 0; i < MAX_NODOSI; i++) {
		free(miSistemaDeFicheros->nodosI[i]);
		miSistemaDeFicheros->nodosI[i] = NULL;
	}
	exit(1);
}
