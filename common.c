#include "common.h"
#include <stdlib.h>
#include <string.h>

int escribeMapaDeBits(MiSistemaDeFicheros* miSistemaDeFicheros) {
	if (lseek(miSistemaDeFicheros->discoVirtual, TAM_BLOQUE_BYTES
			* MAPA_BITS_IDX, SEEK_SET) == (off_t) -1) {
		perror("Falló lseek en escribeMapaDeBits");
		return -1;
	}
	if (write(miSistemaDeFicheros->discoVirtual,
			miSistemaDeFicheros->mapaDeBits, sizeof(BIT) * NUM_BITS) == -1) {
		perror("Falló write en escribeMapaDeBits");
		return -1;
	}
	return 0;
}

int escribeNodoI(MiSistemaDeFicheros* miSistemaDeFicheros, int numNodoI,
		EstructuraNodoI* nodoI) {
	int posNodoI;
	assert(numNodoI < MAX_NODOSI);
	posNodoI = calculaPosNodoI(numNodoI);

	if (lseek(miSistemaDeFicheros->discoVirtual, posNodoI, SEEK_SET)
			== (off_t) -1) {
		perror("Falló lseek en escribeNodoI");
		return -1;
	}
	if (write(miSistemaDeFicheros->discoVirtual, nodoI, sizeof(EstructuraNodoI))
			== -1) {
		perror("Falló write en escribeNodoI");
	}
	sync();
	return 1;
}

/* Inicializa el superbloque */
void initSuperBloque(MiSistemaDeFicheros* miSistemaDeFicheros, int tamDisco) {
	miSistemaDeFicheros->superBloque.tamDiscoEnBloques = tamDisco
			/ TAM_BLOQUE_BYTES;
	miSistemaDeFicheros->superBloque.numBloquesLibres = myQuota(
			miSistemaDeFicheros);

	miSistemaDeFicheros->superBloque.tamSuperBloque
			= sizeof(EstructuraSuperBloque);
	miSistemaDeFicheros->superBloque.tamDirectorio
			= sizeof(EstructuraDirectorio);
	miSistemaDeFicheros->superBloque.tamNodoI = sizeof(EstructuraNodoI);

	miSistemaDeFicheros->superBloque.tamBloque = TAM_BLOQUE_BYTES;
	miSistemaDeFicheros->superBloque.maxTamNombreArchivo
			= MAX_TAM_NOMBRE_ARCHIVO;
	miSistemaDeFicheros->superBloque.maxBloquesPorArchivo
			= MAX_BLOQUES_POR_ARCHIVO;
}

int escribeSuperBloque(MiSistemaDeFicheros* miSistemaDeFicheros) {
	if (lseek(miSistemaDeFicheros->discoVirtual, TAM_BLOQUE_BYTES
			* SUPERBLOQUE_IDX, SEEK_SET) == (off_t) -1) {
		perror("Falló lseek en escribeSuperBloque");
		return -1;
	}
	if (write(miSistemaDeFicheros->discoVirtual,
			&(miSistemaDeFicheros->superBloque), sizeof(EstructuraSuperBloque))
			== -1) {
		perror("Falló write en escribeSuperBloque");
		return -1;
	}
	return 0;
}

int escribeDirectorio(MiSistemaDeFicheros* miSistemaDeFicheros) {
	if (lseek(miSistemaDeFicheros->discoVirtual, TAM_BLOQUE_BYTES
			* DIRECTORIO_IDX, SEEK_SET) == (off_t) -1) {
		perror("Falló lseek en escribeDirectorio");
		return -1;
	}
	if (write(miSistemaDeFicheros->discoVirtual,
			&(miSistemaDeFicheros->directorio), sizeof(EstructuraDirectorio))
			== -1) {
		perror("Falló write en escribeDirectorio");
		return -1;
	}
	return 0;
}

int escribeDatos(MiSistemaDeFicheros* miSistemaDeFicheros, int archivoExterno,
		int numNodoI) {
	int i;
	char buffer[TAM_BLOQUE_BYTES];
	EstructuraNodoI* temp = miSistemaDeFicheros->nodosI[numNodoI];
	int bytesRestantes = miSistemaDeFicheros->superBloque.tamBloque
			- (temp->numBloques * miSistemaDeFicheros->superBloque.tamBloque
					- temp->tamArchivo);
	for (i = 0; i < temp->numBloques - 1; i++) {
		if (read(archivoExterno, &buffer, TAM_BLOQUE_BYTES) == -1) {
			perror("Falló read en escribeDatos");
			return -1;
		}
		if (lseek(miSistemaDeFicheros->discoVirtual, temp->idxBloques[i]
				* TAM_BLOQUE_BYTES, SEEK_SET) == (off_t) -1) {
			perror("Falló lseek en escribeDatos");
			return -1;
		}
		if (write(miSistemaDeFicheros->discoVirtual, &buffer, TAM_BLOQUE_BYTES)
				== -1) {
			perror("Falló write en escribeDatos");
			return -1;
		}
	}
	if (read(archivoExterno, &buffer, bytesRestantes) == -1) {
		perror("Falló read (2) en escribeDatos");
		return -1;
	}
	if (lseek(miSistemaDeFicheros->discoVirtual, temp->idxBloques[i]
			* TAM_BLOQUE_BYTES, SEEK_SET) == (off_t) -1) {
		perror("Falló lseek (2) en escribeDatos");
		return -1;
	}
	if (write(miSistemaDeFicheros->discoVirtual, &buffer, bytesRestantes) == -1) {
		perror("Falló write (2) en escribeDatos");
	}
	return 0;
}

int exportaDatos(MiSistemaDeFicheros* miSistemaDeFicheros, int handle,
		int idxNodoI) {
	int i;
	char buffer[TAM_BLOQUE_BYTES];
	EstructuraNodoI* temp = miSistemaDeFicheros->nodosI[idxNodoI];
	int bytesRestantes = miSistemaDeFicheros->superBloque.tamBloque
			- (temp->numBloques * miSistemaDeFicheros->superBloque.tamBloque
					- temp->tamArchivo);
	for (i = 0; i < miSistemaDeFicheros->nodosI[idxNodoI]->numBloques - 1; ++i) {
		if (write(handle, &buffer, TAM_BLOQUE_BYTES) == -1) {
			perror("Falló write en exportaDatos");
			return -1;
		}

	}

	if (write(handle, &buffer, bytesRestantes) == -1) {
		perror("Falló write (2) en exportaDatos");
		return -1;
	}

	return 0;
}

int calculaPosNodoI(int numNodoI) {
	int whichInodeBlock;
	int whichInodeInBlock;
	int inodeLocation;

	whichInodeBlock = numNodoI / NODOSI_POR_BLOQUE;
	whichInodeInBlock = numNodoI % NODOSI_POR_BLOQUE;

	inodeLocation = (NODOI_IDX + whichInodeBlock) * TAM_BLOQUE_BYTES
			+ whichInodeInBlock * sizeof(EstructuraNodoI);
	return inodeLocation;
}

void initNodosI(MiSistemaDeFicheros* miSistemaDeFicheros) {
	int numNodoI;
	EstructuraNodoI* temp = malloc(sizeof(EstructuraNodoI));

	for (numNodoI = 0; numNodoI < MAX_NODOSI; numNodoI++) {
		leeNodoI(miSistemaDeFicheros, numNodoI, temp);
		if (temp->libre) {
			miSistemaDeFicheros->nodosI[numNodoI] = NULL;
		} else {
			miSistemaDeFicheros->numNodosLibres--;
			miSistemaDeFicheros->nodosI[numNodoI] = malloc(
					sizeof(EstructuraNodoI));
			copiaNodoI(miSistemaDeFicheros->nodosI[numNodoI], temp);
		}
	}
}

int leeNodoI(MiSistemaDeFicheros* miSistemaDeFicheros, int numNodoI,
		EstructuraNodoI* nodoI) {
	int posNodoI;
	assert(numNodoI < MAX_NODOSI);
	posNodoI = calculaPosNodoI(numNodoI);

	lseek(miSistemaDeFicheros->discoVirtual, posNodoI, SEEK_SET);
	read(miSistemaDeFicheros->discoVirtual, nodoI, sizeof(EstructuraNodoI));
	return 1;
}

void copiaNodoI(EstructuraNodoI* dest, EstructuraNodoI* src) {
	int i;

	dest->numBloques = src->numBloques;
	dest->tamArchivo = src->tamArchivo;
	dest->tiempoModificado = src->tiempoModificado;
	dest->libre = src->libre;

	for (i = 0; i < MAX_BLOQUES_POR_ARCHIVO; i++)
		dest->idxBloques[i] = src->idxBloques[i];
}

int buscaNodoLibre(MiSistemaDeFicheros* miSistemaDeFicheros) {
	int i;
	for (i = 0; i < MAX_NODOSI; i++) {
		if (miSistemaDeFicheros->nodosI[i] == NULL)
			return i;
	}
	return -1; // NO hay nodos-i libres. Esto no debería ocurrir.
}

void reservaBloquesNodosI(MiSistemaDeFicheros* miSistemaDeFicheros,
		DISK_LBA idxBloques[], int numBloques) {
	int i = 0;
	int bloqueActual = 0;

	while ((bloqueActual < numBloques) && (i < NUM_BITS)) {
		if (miSistemaDeFicheros->mapaDeBits[i] == 0) {
			miSistemaDeFicheros->mapaDeBits[i] = 1;
			idxBloques[bloqueActual] = i;
			bloqueActual++;
		}
		++i;
	}
}

int buscaPosDirectorio(MiSistemaDeFicheros* miSistemaDeFicheros, char* nombre) {
	int i;

	for (i = 0; i < MAX_ARCHIVOS_POR_DIRECTORIO; i++) {
		if (miSistemaDeFicheros->directorio.archivos[i].libre == false) {
			if (strcmp(nombre,
					miSistemaDeFicheros->directorio.archivos[i].nombreArchivo)
					== 0)
				return i;
		}
	}
	return -1;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Devuelve el no de bloques libres en el FS.

int myQuota(MiSistemaDeFicheros* miSistemaDeFicheros) {
	int freeCount = 0;
	int i;
	// Calculamos el número de bloques libres
	for (i = 0; i < miSistemaDeFicheros->superBloque.tamDiscoEnBloques; i++) {
		// Ahora estamos usando uint para representar cada bit.
		// Podríamos usar todos los bits para mejorar el almacenamiento
		if (miSistemaDeFicheros->mapaDeBits[i] == 0) {
			freeCount++;
		}
	}
	return freeCount;
}
