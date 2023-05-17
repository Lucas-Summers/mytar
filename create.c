#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>

#include "util.h"

#define PATH_MAX 256
#define NAME_MAX 100
#define UGNAME_MAX 32

#define ID_MAX 07777777
#define PERM_MASK 07777

#define MTIME_MAX 12
#define SIZE_SIZE 12
#define ID_SIZE 8


void write_stop_blocks(int tarfile) {
    char *stop_blocks;
    int i;

    if ((stop_blocks = (char *)malloc(sizeof(char) * BLOCK*2)) == NULL) {
        perror("mytar");
        exit(1);
    }

    for (i = 0; i < BLOCK*2; i++) {
        stop_blocks[i] = 0;
    }

    if (write(tarfile, stop_blocks, BLOCK*2) == -1) {
        perror("mytar");
        exit(1);
    }

    free(stop_blocks);
}

int write_header(int tarfile, char *path, struct stat *st, 
                                int verbose, int strict) {
    struct tarheader head;
    struct passwd *pw;
    struct group *gr;
    int i;
    
    memset(&head, 0, BLOCK); 

    if (verbose) {
        printf("%s\n", path);
    }

    if (strlen(path) <= NAME_MAX) {
        strncpy(head.name, path, strlen(path));
    } else {
        i = (strlen(path) - 1) + NAME_MAX;
        while (path[i] != '/') {
            if (path[i] == 0) {
               fprintf(stderr, "%s: path too long\n", path);
               return -1;
            }
            i++;
        }
        strncpy(head.name, path + i + 1, NAME_MAX);
        strncpy(head.prefix, path, i);
    }

    if (st->st_uid > ID_MAX) {
        if (strict) {
            fprintf(stderr, "%s: uid too large\n", path);
            return -1;
        }
        insert_special_int(head.uid, ID_SIZE, st->st_uid);
    } else {
        sprintf(head.uid, "%07o", st->st_uid);
    }

    if (st->st_gid > ID_MAX) {
        if (strict) {
            fprintf(stderr, "%s: gid too large\n", path);
            return -1;
        }
        insert_special_int(head.gid, ID_SIZE, st->st_gid);
    } else {
        sprintf(head.gid, "%07o", st->st_gid);
    }
    
    if (S_ISREG(st->st_mode)) {
        head.typeflag[0] = (char)RFLAG;
        if (st->st_size > SIZE_MAX) {
            if (strict) {
                fprintf(stderr, "%s: size to large\n", path);
                return -1;
            }
            insert_special_int(head.size, SIZE_SIZE, st->st_size);
        } else {
            sprintf(head.size, "%011o", (int) st->st_size);
        }
    } else if (S_ISLNK(st->st_mode)) {
        head.typeflag[0] = (char)LFLAG;
        sprintf(head.size, "%011o", 0);
        readlink(path, head.linkname, 100);
    } else if (S_ISDIR(st->st_mode)) {
        head.typeflag[0] = (char)DFLAG;
        sprintf(head.size, "%011o", 0);
    }

    if (st->st_mtime > MTIME_MAX) {
        if (strict) {
            fprintf(stderr, "%s: mtime too large\n", path);
            return -1;
        }
        insert_special_int(head.mtime, MTIME_MAX, st->st_mtime);
    } else {
        sprintf(head.mtime, "%011o", (int)st->st_mtime);
    }

    strcpy(head.magic, "ustar");
    strcpy(head.version, "00");
    sprintf(head.mode, "%07o", st->st_mode & PERM_MASK);
    

    if (!(gr = getgrgid(st->st_gid))) {
        perror("mytar");
        exit(1);
    }

    if (strlen(gr->gr_name) >= UGNAME_MAX) {
        strncpy(head.gname, gr->gr_name, UGNAME_MAX - 1);
    } else {
        strcpy(head.gname, gr->gr_name);
    }

    if (!(pw = getpwuid(st->st_uid))) {
        perror("mytar");
        exit(1);
    }

    if (strlen(pw->pw_name) >= UGNAME_MAX) {
        strncpy(head.uname, pw->pw_name, UGNAME_MAX - 1);
    } else {
        strcpy(head.uname, pw->pw_name);
    }

    sprintf(head.chksum, "%07o", calculate_checksum((unsigned char *)&head));

    if (write(tarfile, &head, BLOCK) == -1) {
        perror("mytar");
        exit(1);
    }

    return 0;

}

void write_tar(int tarfile, char *path, int verbose, int strict) {
    struct stat st;
    int infile;
    char buf[BLOCK];
    int i;
    DIR *dir;
    struct dirent *dirp;
    char *path_new;

    if (lstat(path, &st) == -1) {
        perror("mytar");
        return;
    }
    

    if (S_ISREG(st.st_mode)) {
        if ((infile = open(path, O_RDONLY)) == -1) {
            perror("mytar");
            exit(1);
        }

        for (i = 0; i < BLOCK; i++) {
            buf[i] = 0;
        }

        if (write_header(tarfile, path, &st, verbose, strict) != -1) {
            while (read(infile, buf, BLOCK) > 0 ) {

                if (write(tarfile, buf, BLOCK) == -1) {
                    perror("mytar");
                    exit(1);
                }

                for (i = 0; i < BLOCK; i++) {
                    buf[i] = 0;
                }
            }
        }
        close(infile);

    } else if (S_ISLNK(st.st_mode)) {
        write_header(tarfile, path, &st, verbose, strict); 

    } else if (S_ISDIR(st.st_mode)) {
        if (strlen(path) > PATH_MAX) {
            fprintf(stderr, "%s: path too long\n", path);
        }

        if ((path_new = (char *)malloc(PATH_MAX)) == NULL) {
            perror("mytar");
            exit(1);
        }

        write_header(tarfile, path, &st, verbose, strict);

        if ((dir = opendir(path)) == NULL) {
            perror("mytar");
            exit(1);
        }

        while ((dirp = readdir(dir)) != NULL) {
            if (strcmp(dirp->d_name, ".") != 0 &&
                    strcmp(dirp->d_name, "..") != 0) {
                if ((strlen(path) + strlen(dirp->d_name)) < PATH_MAX) {
                    strcpy(path_new, path);
                    strcat(path_new, "/");
                    strcat(path_new, dirp->d_name);
                    write_tar(tarfile, path_new, verbose, strict);
                } else {
                    fprintf(stderr, "%s: path too long\n", path);
                }
            }
        }
        closedir(dir);
        free(path_new);
    }

    return;
}

void create(char *filename, char **paths, int npaths, int verbose, int strict) {
    int tarfile;
    int i = 0;
    char *path;

    if ((tarfile = open(filename, O_RDWR | O_CREAT | O_TRUNC, 
                            S_IRUSR | S_IWUSR | S_IRGRP)) == -1) {
        perror("mytar");
        exit(1);
    }
   
    while (npaths > 0) {
        path = paths[i++];
        if (path[strlen(path) - 1] == '/') {
            path[strlen(path) - 1] = '\0';
        }

        write_tar(tarfile, path, verbose, strict);
        npaths--;
    } 

    write_stop_blocks(tarfile);
    close(tarfile);
}
