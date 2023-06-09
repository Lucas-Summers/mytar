/*
 * file: mytar.c
 * 
 * processes all the args and files/paths
 * based on c/x/t args, chooses which mode to go into
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "create.h"
#include "list.h"
#include "extract.h"

#define OPSMIN 2
#define OPSMAX 4

/* for position in argv */
#define OPS 1
#define TFILE 2
#define PATHS 3

/*
 * print the usage message error
 */
void print_usage(void) {
    fprintf(stderr, "usage: mytar [ctxvS]f tarfile [ path [ ... ] ]\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    int verbose=0, strict=0; /* booleans for v and S option */
    int i, j;
    char **paths;
    char *file;
    int nops;
    
    /* less args then needed */
    if (argc < PATHS) {
        print_usage();
    }
    
    /* catch the # of ops being to big/small */
    nops = strlen(argv[OPS]);
    if (nops < OPSMIN || nops > OPSMAX) {
        print_usage();
    }
    
    /* first op must be c, t, or x */
    if (argv[OPS][0] != 'c' && argv[OPS][0] != 't' && argv[OPS][0] != 'x') {
        print_usage(); 
    }
    
    /* loop through rest of args and check for v or S */
    for(i = 1; argv[OPS][i] != 'f'; i++) {
        /* last op must be f */
        if (argv[1][i] == '\0') {
            print_usage();
        } else if (argv[OPS][i] == 'v') {
            verbose = 1;
        } else if (argv[OPS][i] == 'S') {
            strict = 1;
        } else {
            /* catch an unknown option */
            fprintf(stderr, "unknown option: %c\n", argv[OPS][i]);
            print_usage();
        }
    }
    
    /* the .tar archive file */
    file = argv[TFILE];
    
    /* no paths specified */ 
    if ((argc - PATHS) == 0) {
        paths = NULL;
        i = 0;
    } else {
        if ((paths = malloc((sizeof(char *)) * (argc - PATHS))) == NULL) {
            perror("mytar");
            exit(EXIT_FAILURE);
        }

        for (i=0, j=PATHS; j < argc; i++, j++) {
            paths[i] = argv[j];
        } 
    } 
    
    /* find out which mode to enter based on cvx */
    switch(argv[OPS][0]) {
        case 'c':
            create(file, paths, i, verbose, strict);
            break;
        case 't':
            list(file, paths, i, verbose, strict);
            break;
        case 'x':
            extract(file, paths, i, verbose, strict); 
            break;
    }

    free(paths);
    return 0;
}
