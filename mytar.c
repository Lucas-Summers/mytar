#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "create.h"
#include "list.h"
#include "extract.h"

#define OPS 1
#define OPSMIN 2
#define OPSMAX 4
#define TFILE 2
#define PATHS 3

void print_usage(void);

int main(int argc, char *argv[]) {
    int verbose=0, strict=0;
    int i, j;
    char **paths;
    char *file;
    int nops;

    if (argc < PATHS) {
        print_usage();
    }
    
    nops = strlen(argv[OPS]);

    if (nops < OPSMIN || nops > OPSMAX) {
        print_usage();
    }

    if (argv[OPS][0] != 'c' && argv[OPS][0] != 't' && argv[OPS][0] != 'x') {
        print_usage(); 
    }

    for(i = 1; argv[OPS][i] != 'f'; i++) {
        if (argv[1][i] == '\0') {
            print_usage();
        } else if (argv[OPS][i] == 'v') {
            verbose = 1;
        } else if (argv[OPS][i] == 'S') {
            strict = 1;
        } else {
            print_usage();
        }
    }

    file = argv[TFILE];
    
    if ((argc - PATHS) == 0) {
        paths = NULL;
        i = 0;
    } else {
        if ((paths = malloc((sizeof(char *)) * (argc - PATHS))) == NULL) {
            perror("mytar");
            exit(1);
        }

        for (i=0, j=PATHS; j < argc; i++, j++) {
            paths[i] = argv[j];
        } 
    } 

    switch(argv[OPS][0]) {
        case 'c':
            create(file, paths, i, verbose, strict);
            break;
        case 't':
            /* list(file, paths, i, verbose, strict); */
            break;
        case 'x':
            /* extract(file, paths, i, verbose, strict); */
            break;
    }

    free(paths);
    return 0;
}

void print_usage(void) {
    fprintf(stderr, "usage: mytar [ctxvS]f tarfile [ path [ ... ] ]\n");
    exit(1);
}
