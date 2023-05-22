/* 
 * file: create.c
 * 
 * create mode for mytar (specified by the c argument)
 *
 * if the archive file exists, it is truncated to zero length
 *
 * Then all the remaining arguments on the command line are taken as paths to 
 * be added to the archive
 *
 * If a given path is a directory, 
 * that directory and all the files and directories below it are added
 * to the archive.
 *
 */

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

/*
 * write two 0 blocks to signal the end of the archive as per specification
 *
 */
void write_stop_blocks(int tarfile) {
    char *stop_blocks;
    int i;
    
    /* two 512 byte blocks */
    if ((stop_blocks = (char *)malloc(sizeof(char) * BLOCK*2)) == NULL) {
        perror("mytar");
        exit(EXIT_FAILURE);
    }
    
    /* zero out the blocks */
    for (i = 0; i < BLOCK*2; i++) {
        stop_blocks[i] = 0;
    }
    
    /* should be at the end of the file */
    if (write(tarfile, stop_blocks, BLOCK*2) == -1) {
        perror("mytar");
        exit(EXIT_FAILURE);
    }

    free(stop_blocks);
}

/* 
 * populates the tarheader struct with all the file metadata needed
 *
 */
int write_header(int tarfile, char *path, struct stat *st, 
                                int verbose, int strict) {
    struct tarheader head;
    struct passwd *pw;
    struct group *gr;
    int i;
    
    /* create a clean slate in case we don't write in every position */
    memset(&head, 0, BLOCK); 
    
    /* if v arg, list out the files as they are added */
    if (verbose) {
        printf("%s\n", path);
    }
    
    /* if path fits into the name field put it there */
    if (strlen(path) <= NAME_MAX_) {
        strncpy(head.name, path, strlen(path));
    /* else, we put the path into the prefix and filename in the name */
    } else {
        /* find the index to split the path by */
        i = (strlen(path) - 1) - NAME_MAX_;
        while (path[i] != '/') {
            if (path[i] == 0) {
               fprintf(stderr, "%s: path cannot be partitioned\n", path);
               return -1;
            }
            i++;
        }
        strncpy(head.name, path + i + 1, NAME_MAX_);
        strncpy(head.prefix, path, i);
    }
    
    /* populate uid field octal with the file uid */
    if (st->st_uid > ID_MAX) {
        /* if S arg, just throw an error
         * else, just encode it as a regular binary number
         */
        if (strict) {
            fprintf(stderr, "%s: uid too large\n", path);
            return -1;
        }
        if (insert_special_int(head.uid, ID_SIZE, st->st_uid) == -1) {
            perror("mytar");
            exit(EXIT_FAILURE);
        }
    } else {
        /* sprintf helps us format it as a specific length octal */
        sprintf(head.uid, "%07o", st->st_uid);
    }

    /* populate gid field octal with the file gid (same process as uid) */
    if (st->st_gid > ID_MAX) {
        if (strict) {
            fprintf(stderr, "%s: gid too large\n", path);
            return -1;
        }

        /* encode it as a regular binary number if too large */
        if (insert_special_int(head.gid, ID_SIZE, st->st_gid)) {
            perror("mytar");
            exit(EXIT_FAILURE);
        }
    } else {
        sprintf(head.gid, "%07o", st->st_gid);
    }
    
    /* is file regular? */
    if (S_ISREG(st->st_mode)) {
        /* set type flag to '0' */
        head.typeflag[0] = (char)RFLAG;
        if (st->st_size > SIZE_MAX_) {
            /* if S arg, return an error 
             * else, just use gnu tar special int
             */
            if (strict) {
                fprintf(stderr, "%s: size to large\n", path);
                return -1;
            }
            insert_special_int(head.size, SIZE_SIZE, st->st_size);
        } else {
            sprintf(head.size, "%011o", (int)st->st_size);
        }
    /* is file symlink? */
    } else if (S_ISLNK(st->st_mode)) {
        /* set type flag to '2' */
        head.typeflag[0] = (char)LFLAG;
        /* size is zero per specification */
        sprintf(head.size, "%011o", 0);
        /* put the value of the link into linkname field */
        readlink(path, head.linkname, 100);
    /* is file a directory? */
    } else if (S_ISDIR(st->st_mode)) {
        /* set type flag to '5' */
        head.typeflag[0] = (char)DFLAG;
        /* size is zero per specification */
        sprintf(head.size, "%011o", 0);
    }
    
    /* populate mtime field with files mtime */
    if (st->st_mtime > MTIME_MAX) {
        if (strict) {
            fprintf(stderr, "%s: mtime too large\n", path);
            return -1;
        }
        insert_special_int(head.mtime, MTIME_SIZE, st->st_mtime);
    } else {
        sprintf(head.mtime, "%011o", (int)st->st_mtime);
    }
    
    /* magic and version fields always the same */
    strcpy(head.magic, "ustar");
    strcpy(head.version, "00");
    
    /* AND mode with mask to clear everything but the perms */
    sprintf(head.mode, "%07o", st->st_mode & MODE_MASK);
    
    /* retrieve the group name with file gid */
    if (!(gr = getgrgid(st->st_gid))) {
        perror("mytar");
        exit(EXIT_FAILURE);
    }
    
    /* truncate name if necessary to fit within field */
    if (strlen(gr->gr_name) >= UGNAME_MAX) {
        strncpy(head.gname, gr->gr_name, UGNAME_MAX - 1);
    } else {
        strcpy(head.gname, gr->gr_name);
    }

    /* retrieve user name with the files uid */
    if (!(pw = getpwuid(st->st_uid))) {
        perror("mytar");
        exit(EXIT_FAILURE);
    }

    /* truncate name if necessary to fit within field */
    if (strlen(pw->pw_name) >= UGNAME_MAX) {
        strncpy(head.uname, pw->pw_name, UGNAME_MAX - 1);
    } else {
        strcpy(head.uname, pw->pw_name);
    }
    
    /* calculate checksum from the header created and populate the field */
    sprintf(head.chksum, "%07o", calculate_checksum((unsigned char *)&head));
    
    /* write the header to the outfile */
    if (write(tarfile, &head, BLOCK) == -1) {
        perror("mytar");
        exit(EXIT_FAILURE);
    }

    return 0;
}

/*
 * write archive blocks to the tarfile with the paths passed in
 *
 */
void write_tar(int tarfile, char *path, int verbose, int strict) {
    struct stat st;
    int infile;
    char buf[BLOCK];
    int i;
    DIR *dir;
    struct dirent *dirp;
    char *path_new;
    char temp[LINK_MAX];

    /* to be used when writing the header */
    if (lstat(path, &st) == -1) {
        perror(path);
        return;
    }
    
    /* is file regular?
     * then write the header and its data in blocks
     */
    if (S_ISREG(st.st_mode)) {
        /* skip writing file if we can't open for reading */
        if ((infile = open(path, O_RDONLY)) == -1) {
            perror(path);
            return;
        }

        /* clear the buf so if file doesn't fit perfectly into block
         * it will still look good
         */
        for (i = 0; i < BLOCK; i++) {
            buf[i] = 0;
        }

        if (write_header(tarfile, path, &st, verbose, strict) != -1) {
            /* read in a block and write it to the tarfile */
            while (read(infile, buf, BLOCK) > 0 ) {

                if (write(tarfile, buf, BLOCK) == -1) {
                    perror("mytar");
                    exit(EXIT_FAILURE);
                }
                
                /* zero out the buf again */
                for (i = 0; i < BLOCK; i++) {
                    buf[i] = 0;
                }
            }
        }
        close(infile);
    /* is file link?
     * just write the header, all the data is in the linkname
     */
    } else if (S_ISLNK(st.st_mode)) {
        /* skip writing link if we cannot open */
        if (readlink(path, temp, LINK_MAX) == -1) {
            perror(path);
            return;
        }
        write_header(tarfile, path, &st, verbose, strict); 
    /* is file a dir?
     * write the header then recurse on all the entries
     */
    } else if (S_ISDIR(st.st_mode)) {
        /* catch path too long */
        if (strlen(path) > PATH_MAX_) {
            fprintf(stderr, "%s: path too long\n", path);
        }
        
        /* something to put the new path into */
        if ((path_new = (char *)malloc(PATH_MAX_)) == NULL) {
            perror("mytar");
            exit(EXIT_FAILURE);
        }
        
        /* skip writing dir to archive if cannot open */
        if ((dir = opendir(path)) == NULL) {
            perror(path);
            return;
        }

        /* add add slash to match mytar*/
        strcat(path, "/");
        write_header(tarfile, path, &st, verbose, strict);
        
        
        /* go through all directory entries and recurse */
        while ((dirp = readdir(dir)) != NULL) {
            /* don't recurse on . or .. !!! */
            if (strcmp(dirp->d_name, ".") != 0 &&
                    strcmp(dirp->d_name, "..") != 0) {
                if ((strlen(path) + strlen(dirp->d_name)) < PATH_MAX_) {
                    strcpy(path_new, path);
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
}

/* 
 * the create command mode accessed by the main function
 */
void create(char *filename, char **paths, int npaths, int verbose, int strict) {
    int tarfile;
    int i = 0;
    char *path;
    
    /* create the tarfile with the perms rw_r____ as specified */
    if ((tarfile = open(filename, O_RDWR | O_CREAT | O_TRUNC, 
                            S_IRUSR | S_IWUSR | S_IRGRP)) == -1) {
        perror(filename);
        exit(EXIT_FAILURE);
    }
    
    /* go through all the paths passed in and archive them */
    while (npaths > 0) {
        path = paths[i++];
        /* make sure we don't get a double slash */
        if (path[strlen(path) - 1] == '/') {
            path[strlen(path) - 1] = '\0';
        }

        write_tar(tarfile, path, verbose, strict);
        npaths--;
    } 
    
    /* finish off the archive with the stop blocks */
    write_stop_blocks(tarfile);
    close(tarfile);
}
