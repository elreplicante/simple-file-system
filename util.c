#include "util.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

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
	EstructuraNodoI nodoActual; //auxiliar para inicializacions
	nodoActual.libre = 1;
	// Escribimos nodoActual MAX_NODOSI veces en disco
	for (i = 0; i < MAX_NODOSI; i++) {
		escribeNodoI(miSistemaDeFicheros, i, &nodoActual);
		//Poner array nodos-i a NULL
		miSistemaDeFicheros->nodosI[i] = NULL;
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
	if (stStat.st_size > miSistemaDeFicheros->superBloque.numBloquesLibres
			* TAM_BLOQUE_BYTES) {
		fprintf(stderr, "No hay suficiente espacio en disco\n");
		return 3;
	}

	/// Comprobamos que el tamaño total es suficientemente pequeño
	/// para ser almacenado en MAX_BLOCKS_PER_FILE
	if (stStat.st_size > (TAM_BLOQUE_BYTES * MAX_BLOQUES_POR_ARCHIVO)) {
		fprintf(stderr, "El archivo a copiar es demasido grande\n");
		return 4;
	}

	/// Comprobamos que la longitud del nombre del archivo es adecuada
	if (strlen(nombreArchivoInterno) > MAX_TAM_NOMBRE_ARCHIVO) {
		fprintf(stderr, "Nombre de archivo demasiado grande\n");
		return 5;
	}

	/// Comprobamos que el fichero no existe ya
	if (buscaPosDirectorio(miSistemaDeFicheros, nombreArchivoInterno) != -1) {
		fprintf(stderr, "El archivo a copiar ya existe\n");
		return 6;
	}

	/// Comprobamos si existe un nodo-i libre
	if (nodoLibre == -1) {
		fprintf(stderr, "No existen nodos-i libres\n");
		return 7;
	}

	/// Comprobamos que todavía cabe un archivo en el directorio (MAX_ARCHIVOS_POR_DIRECTORIO)
	if (miSistemaDeFicheros->directorio.numArchivos
			== MAX_ARCHIVOS_POR_DIRECTORIO) {
		fprintf(stderr, "No caben mas archivos en el directorio\n");
		return 8;
	}

	/// Actualizamos toda la información:
	/// mapa de bits, directorio, nodo-i, bloques de datos, superbloque ...
	/****************Nodo-i***********************/
	EstructuraNodoI *nodo = malloc(sizeof(EstructuraNodoI));

	nodo->tamArchivo = stStat.st_size;
	nodo->numBloques = ((stStat.st_size + (TAM_BLOQUE_BYTES - 1))
			/ TAM_BLOQUE_BYTES);
	nodo->libre = 0;
	nodo->tiempoModificado = time(0);
	miSistemaDeFicheros->nodosI[nodoLibre] = nodo;
	miSistemaDeFicheros->numNodosLibres--;

	reservaBloquesNodosI(miSistemaDeFicheros, nodo->idxBloques,
			nodo->numBloques);

	escribeNodoI(miSistemaDeFicheros, nodoLibre, nodo);
	/***************bloque de datos*****************/

	escribeDatos(miSistemaDeFicheros, handle, nodoLibre);

	escribeMapaDeBits(miSistemaDeFicheros);

	miSistemaDeFicheros->directorio.numArchivos++;
	miSistemaDeFicheros->directorio.archivos[nodoLibre].libre = 0;
	strcpy(miSistemaDeFicheros->directorio.archivos[nodoLibre].nombreArchivo,
			nombreArchivoInterno);
	escribeDirectorio(miSistemaDeFicheros);
	miSistemaDeFicheros->superBloque.numBloquesLibres -= nodo->numBloques;
	escribeSuperBloque(miSistemaDeFicheros);

	sync();
	close(handle);
	return 0;
}

int myExport(MiSistemaDeFicheros* miSistemaDeFicheros,
		char* nombreArchivoInterno, char* nombreArchivoExterno) {
	int handle;

	/// Buscamos el archivo nombreArchivoInterno en miSistemaDeFicheros
	int posDirectorio = buscaPosDirectorio(miSistemaDeFicheros,
			nombreArchivoInterno);
	if (posDirectorio == -1) {
		perror(" El archivo a exportar no existe");
		return 1;
	}
	// ...

	/// Si ya existe el archivo nombreArchivoExterno en linux preguntamos si sobreescribir
	handle = open(nombreArchivoExterno, O_WRONLY);
	if (handle != -1) {
		printf("El archivo ya existe,desea sobreescribir?(y/N):\n");
		char option;
		scanf("%c", &option);
		if (option == 'n' || option == 'N') {
			printf("%s", "Escribe otro nombre de archivo:\n");
			scanf("%s", nombreArchivoExterno);
			handle = creat(nombreArchivoExterno, O_WRONLY);
		} else if (option == 'y' || option == 'Y') {
			handle = open(nombreArchivoExterno, O_WRONLY|O_TRUNC, 0664);
		}
	} else {
		handle = creat(nombreArchivoExterno, 0644);
	}

	/// Copiamos bloque a bloque del archivo interno al externo
	int idxNodoI =
			miSistemaDeFicheros->directorio.archivos[posDirectorio].idxNodoI;

	exportaDatos(miSistemaDeFicheros, handle, idxNodoI);
	
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
	int posDirectorio = buscaPosDirectorio(miSistemaDeFicheros, nombreArchivo);
	if (posDirectorio == -1) {
		fprintf(stderr, "El archivo a borrar no existe\n");
		return 1;
	}

	// Obtiene el nodo-i asociado y lo actualiza
	int posNodoI =
			miSistemaDeFicheros->directorio.archivos[posDirectorio].idxNodoI;
	EstructuraNodoI *nodoI = miSistemaDeFicheros->nodosI[posNodoI];
	nodoI->libre = 1;

	// Actualiza el superbloque (numBloquesLibres) y el mapa de bits
	miSistemaDeFicheros->superBloque.numBloquesLibres += nodoI->numBloques;
	int i;
	for (i = 0; i < nodoI->numBloques; i++) {
		miSistemaDeFicheros->mapaDeBits[nodoI->idxBloques[i]] = 0;
	}
	// Libera el puntero y lo hace NULL


	// Actualiza el archivo
	miSistemaDeFicheros->directorio.archivos[posDirectorio].libre = 1;
	// Finalmente, actualiza en disco el directorio, nodoi, mapa de bits y superbloque

	escribeNodoI(miSistemaDeFicheros, posNodoI, nodoI);
	escribeMapaDeBits(miSistemaDeFicheros);
	escribeSuperBloque(miSistemaDeFicheros);
	// ...
	free(nodoI);

	return 0;
}

void myLs(MiSistemaDeFicheros* miSistemaDeFicheros) {
	int numArchivosEncontrados = 0;
	//EstructuraNodoI nodoActual;
	int i;
	// Recorre el sistema de ficheros, listando los archivos encontrados
	printf("%s\n", "Lista de archivos");

	for (i = 0; i < MAX_ARCHIVOS_POR_DIRECTORIO; i++) {
		if (miSistemaDeFicheros->directorio.archivos[i].libre == 0) {
			printf("%s\t",
					miSistemaDeFicheros->directorio.archivos[i].nombreArchivo);
			printf("%d\t", miSistemaDeFicheros->nodosI[i]->tamArchivo);

			struct tm *tlocal = localtime(
					&miSistemaDeFicheros->nodosI[i]->tiempoModificado);
			char output[128];
			strftime(output, 128, "%d/%m/%y %H:%M:%S", tlocal);
			printf("%s\n", output);

			numArchivosEncontrados++;
		}

	}

	if (numArchivosEncontrados == 0) {
		printf("Directorio vacío\n");
	} else {
		printf("Número total de archivos:%d\n",
				miSistemaDeFicheros->directorio.numArchivos);
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
