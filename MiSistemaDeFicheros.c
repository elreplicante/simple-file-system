#include "common.h"
#include "parse.h"
#include "util.h"
#include <readline/readline.h>

int main(int argc, char** argv) {
    MiSistemaDeFicheros miSistemaDeFicheros;
    miSistemaDeFicheros.numNodosLibres = MAX_NODOSI;

    char* lineaComando;
    parseInfo* info; // Almacena toda la información que retorna el parser
    struct commandType* comando; // Almacena el comando y la lista de argumentos
    int ret; // Código de retorno de las llamadas a funciones

    if ((argc == 4) && (strcmp(argv[1],"-mkfs")==0)) {
        // ./MiSistemaDeFicheros -mkfs tamDisco nombreArchivo
    	ret = myMkfs(&miSistemaDeFicheros, atoi(argv[2]), argv[3]);
        if (ret) {
            fprintf(stderr, "Incapaz de formatear, código de error: %d\n", ret);
            exit(-1);
        }
    } else {
        fprintf(stderr, "Error, debes introducir el tamaño del disco y su nombre: ./MiSistemaDeFicheros -mkfs tamDisco nombreArchivo\n");
        exit(-1);
    }
    initNodosI(&miSistemaDeFicheros);
    fprintf(stderr, "Sistema de ficheros disponible\n");

    while (1) {
        lineaComando = readline("% ");
        if (lineaComando == NULL) {
            fprintf(stderr, "Incapaz de leer un comando\n");
            continue;
        }
        // Llamamos al parser
        info = parse(lineaComando);
        if (info == NULL) {
            free(lineaComando);
            continue;
        }
        // Obtenemos el comando
        comando = &info->CommArray[0];
        if ((comando == NULL) || (comando->command == NULL)) {
            free_info(info);
            free(lineaComando);
            continue;
        }

        if (strncmp(comando->command, "import", strlen("import")) == 0) { // IMPORT
            if (comando->VarNum != 3) {
                fprintf(stderr, "import nombreArchivoExterno nombreArchivoInterno\n");
            } else {
            	ret = myImport(comando->VarList[1], &miSistemaDeFicheros, comando->VarList[2]);
                if (ret) {
                    fprintf(stderr, "Incapaz de importar el fichero externo %s en nuestro sistema de ficheros como %s. código de error: %d\n", comando->VarList[1], comando->VarList[2], ret);
                }
            }
        } else if (strncmp(comando->command, "export", strlen("export")) == 0) { // EXPORT
            if (comando->VarNum != 3) {
                fprintf(stderr, "export nombreArchivoInterno nombreArchivoExterno \n");
            } else {
            	ret = myExport(&miSistemaDeFicheros, comando->VarList[1], comando->VarList[2]);
                if (ret) {
                    fprintf(stderr, "Incapaz de exportar el archivo interno %s a el archivo externo %s, código de error: %d\n", comando->VarList[1], comando->VarList[2], ret);
                }
            }
        } else if (strncmp(comando->command, "rm", strlen("rm")) == 0) { // RM
            if (comando->VarNum != 2) {
                fprintf(stderr, "rm nombreArchivo\n");
            } else {
            	ret = myRm(&miSistemaDeFicheros, comando->VarList[1]);
                if (ret) {
                    fprintf(stderr, "Incapaz de borrar el archivo %s, código de error: %d\n", comando->VarList[1], ret);
                }
            }
        } else if (strncmp(comando->command, "ls", strlen("ls")) == 0) { // LS
            myLs(&miSistemaDeFicheros);
        } else if (strncmp(comando->command, "quota", strlen("quota")) == 0) { // QUOTA
            int free_blocks = myQuota(&miSistemaDeFicheros);
            fprintf(stderr, "Espacio libre: %d bytes, %d bloques\n", free_blocks * TAM_BLOQUE_BYTES, free_blocks);
        } else if (strncmp(comando->command, "exit", strlen("exit")) == 0) { // EXIT
        	myExit(&miSistemaDeFicheros);
        } else {
            fprintf(stderr, "Comando desconocido: %s\n", comando->command);
            fprintf(stderr, "\tPrueba con: import, export, ls, rm, quota, exit\n");
        }
        free_info(info);
        free(lineaComando);
    }
}
