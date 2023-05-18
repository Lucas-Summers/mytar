#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#include "util.h"

#define PERM_MASK 256


void list(char *filename, char **paths, int npaths, int verbose, int strict) {
    char *c;
    int tarfile;
    struct tarheader head;

    if((c = strrchr(filename,'.')) != NULL ) {
        if(strcmp(point,".tar") != 0) {
            fprintf(stderr, "%s: file must be .tar\n", filename);
        }
    } else {
        perror("mytar");
        exit(1);
    }

    if ((tarfile = open(filename, O_RDONLY)) == -1) {
        perror(filename);
        exit(1);
    }

    while (read(tarfile, &head, BLOCK) > 0) {
        long size;
        int chksum, expected_chksum, next_chksum, next_expected_chksum;
        char *name;

        size = strtol(head.size, NULL, OCTAL);
        chksum = strtol(head.chksum, NULL, OCTAL);
        expected_chksum = calculate_chksum((unsigned char *)&head);

        if (chksum == 0 && expected_chksum == 256) {
            if (read(tarfile, &head, BLOCK) == -1) {
                perror("mytar");
                exit(1);
            }

            next_chksum = strtol(head.chksum, NULL, OCTAL);
            next_expected_chksum = calculate_chksum((unsigned char *)&head);

            if (next_chksum == 0 && next_expected_chksum == 256) {
                return 0;
            } else {
                fprintf(stderr, "%s: currupted archive\n", filename);
                exit(1);
            }
        }

        if (chksum != expected_chksum) {
            fprintf(stderr, "%s: currupted archive\n", filename);
            exit(1);
        }

        if (strict) {
            if (strcmp("ustar", head.magic) != 0) {
                fprintf(stderr, "header magic is not correct\n");
                exit(1);
            }
            if (strncmp("00", head.version, 2) != 0) {
                fprintf(stderr, "header version is not correct\n");
                exit(1);
            }
        } else {
            if (strncmp("ustar", head.magic, 5) != 0) {
                fprintf(stderr, "header magic is not correct\n");
                exit(1);
            }
        }
        
        if ((name = (char *)malloc(sizeof(char)*PATH_MAX_ + 1)) == NULL) {
            perror("mytar");
            exit(1);
        }
        
        if (head.prefix[0] != 0) {
            strncpy(name, &(head.prefix), sizeof(head.prefix));
        }

        strcat(name, &(head.name), sizeof(head.name));



        if (!verbose) {
            printf("%s\n", name);      
        } else {
            char *perms = "-rwxrwxrwx";
            int mode;
            int perm_mask = PERM_MASK;
            int i;
            
            mode = strtol(head.mode, NULL, OCTAL);
            if (*(head.typeflag) == DFLAG) {
                perms[0] = 'd'; 
            } else if (*(head.typeflag) == LFLAG) {
                perms[0] = 'l';
            }
            
            for (i = 0; i < 9; i++) {
                if (!(mode & mask)) {
                    perms[1+i] = '-';
                }
                mask <<= 1;
            }

            int mtime_;
            struct tm *mtm;
            mtime_ = strtol(head.mtime, NULL, OCTAL);
            mtm = localtime(&mtime_);
                        

            printf("%10.10s %17.17s %8ld %16.16s %s\n",
                    perms, owner_group, size, mtime, name);
        }
         
    }
}
