#ifndef PARSE_H
#define	PARSE_H

#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_VAR_NUM 10
#define PIPE_MAX_NUM 10
#define FILE_MAX_SIZE 40

struct commandType {
    char *command;
    char *VarList[MAX_VAR_NUM];
    int VarNum;
};

/* parsing information structure */
typedef struct {
    int boolInfile; /* boolean value - infile specified */
    int boolOutfile; /* boolean value - outfile specified */
    int boolBackground; /* run the process in the background? */
    struct commandType CommArray[PIPE_MAX_NUM];
    int pipeNum;
    char inFile[FILE_MAX_SIZE]; /* file to be piped from */
    char outFile[FILE_MAX_SIZE]; /* file to be piped into */
} parseInfo;

/* the function prototypes */
parseInfo *parse(char *);
void free_info(parseInfo *);
void print_info(parseInfo *);

#endif	/* PARSE_H */

