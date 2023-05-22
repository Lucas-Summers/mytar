/*
 * file: list.c
 *
 * list (table of contents) mode for mytar (argument t)
 * lists the contents of the given archive file, in order, one per line
 *
 * If no names are given on the command line, mytar, 
 * lists all the files in the archive
 * If a name or names are given on the command line,
 * mytar will list the given path and any and all descendents of it
 *
 * If the verbose (’v’) option is set, 
 * mytar gives expanded information about each file as it lists them.
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <stdint.h>
#include <time.h>

#include "util.h"

/*
 * seek to the next header by jumping over the file contents
 */
void next_header(int tarfile, long size) {
    /* skip links and directories */
    if (size > 0) {
        /* seek by the number of blocks it takes to house the size */
        if (lseek(tarfile, (size / BLOCK + 1) * BLOCK, SEEK_CUR) == -1) {
            perror("mytar");
            exit(EXIT_FAILURE);
        } 
    } 
}

/*
 * returns the name from the header provided
 * if paths are passed in, make sure any path is within or equal to the name
 * else, return no name (NULL)
 */
char *get_name(struct tarheader *head, char **paths, int npaths) {
    char *name;
    int i;
    int found = 0;
    
    /* leave room for / and /0 */
    if ((name = calloc(PATH_MAX_ + 2, sizeof(char))) == NULL) {
        perror("mytar");
        exit(EXIT_FAILURE);
    }
    
    /* if prefix is there, add prefix and name together */
    if (head->prefix[0] != 0) {
        strncpy(name, head->prefix, sizeof(head->prefix));
        strcat(name, "/");
        strncat(name, head->name, sizeof(head->name));
    /* else, just use the name */
    } else {
        strncpy(name, head->name, sizeof(head->name));
    }
    
    /* check if any path arg is in the name */
    if (paths != NULL) {
        for (i = 0; i < npaths; i++) {
            if (strncmp(name, paths[i], strlen(paths[i])) == 0) {
                found = 1; /* bool is used so that we can search every path */
                break;
            }
        }
        /* if none found, return NULL to let the caller know */
        if (!found) {
            free(name);
            return NULL;
        }
    }

    return name;
}

/*
 * returns the file perms formatted in a readable string like ls -l
 */
char *get_perms(struct tarheader *head) {
    char *perms;
    int mode;
    int mask = PERM_MASK;
    int i;
    
    /* populate with the default (all perms provided) */
    if ((perms = (char *)malloc(sizeof(char)*(PERM_STRLEN+1))) == NULL) {
        perror("mytar");
        exit(EXIT_FAILURE);
    }
    strcpy(perms, "-rwxrwxrwx");
    
    /* convert mode back to decimal */
    mode = strtol(head->mode, NULL, OCTAL);

    /* if its a dir, put a d at the front */
    if (*(head->typeflag) == DFLAG) {
        perms[0] = 'd'; 
    /* if its a link, put an l at the front */
    } else if (*(head->typeflag) == LFLAG) {
        perms[0] = 'l';
    }
    
    /* if the bit isn't set, remove the corresponding per from the string */
    for (i = 0; i < PERM_STRLEN-1; i++) {
        if (!(mode & mask)) {
            perms[1+i] = '-';
        }
        mask >>= 1;
    }

    return perms;
}

/* 
 * returns the file mtime formatted in a readable string like ls -l
 */
char *get_mtime(struct tarheader *head) {
    long mtm;
    char *mtime;
    struct tm *tm;
    
    /* convert mtime back to decimal */
    mtm = strtol(head->mtime, NULL, OCTAL);

    /* if the special bit is set, recover the regular number instead */ 
    if (mtm & SPECIAL_INT_MASK) {
        mtm = extract_special_int(head->mtime, sizeof(head->mtime));
    }
    
    /* the final string must fit within 17 chars because of the format */
    if ((mtime = calloc(MTIME_STRLEN+1, sizeof(char))) == NULL) {
        perror("mytar");
        exit(EXIT_FAILURE);
    }
    
    /* create the time structure to be able to format */
    tm = localtime(&mtm);
    
    /* formats to a string like 2004-05-10 10:43 */
    if (strftime(mtime, MTIME_STRLEN+1, "%Y-%m-%d %H:%M", tm) == 0) {
        perror("mypwd");
        exit(EXIT_FAILURE);
    }

    return mtime;
}

/*
 * returns the owner of the file formatted as user/group like ls -l
 */
char *get_owner(struct tarheader *head) {
    char *owner;
    long uid, gid;
    
    /* must fit into 64 chars by the specification */
    if ((owner = calloc(OWNER_STRLEN+1, sizeof(char))) == NULL) {
        perror("mytar");
        exit(EXIT_FAILURE);
    }
    
    /* prefer to use the user name and group name in the header */
    if (head->uname[0] && head->gname[0]) {
        /* create the final string with the username and group name */
        strcat(owner, head->uname);
        strcat(owner, "/");
        strcat(owner, head->gname);
    /* if not, just use the uid and gid */
    } else {
        uid = strtol(head->uid, NULL, OCTAL);
        if (uid & SPECIAL_INT_MASK) {
            extract_special_int(head->uid, sizeof(head->uid));
        }

        gid = strtol(head->gid, NULL, OCTAL);
        if (gid & SPECIAL_INT_MASK) {
            extract_special_int(head->gid, sizeof(head->gid));
        }
        /* format uid/gid into a string */
        snprintf(owner, OWNER_STRLEN+1, "%ld/%ld", uid, gid);
    }

    return owner;
}

/*
 * returns the size of file as a long int per the header
 */
long get_size(struct tarheader *head) {
    long size;

    /* convert back to decimal */
    size = strtol(head->size, NULL, OCTAL);
    /* if the special bit is set, extract the regular number instead */
    if (size & SPECIAL_INT_MASK) {
        size = extract_special_int(head->size, sizeof(head->size));
    }

    return size;
}

/*
 * list command mode accessed by the main function
 */
void list(char *filename, char **paths, int npaths, int verbose, int strict) {
    char *c;
    int tarfile;
    struct tarheader head;

    char *name;
    long size;
    char *perms;
    char *mtime;
    char *owner;
    
    /* check if tarfile ends in .tar */
    if((c = strrchr(filename,'.')) != NULL ) {
        if(strcmp(c,".tar") != 0) {
            fprintf(stderr, "%s: file must be .tar\n", filename);
        }
    } else {
        perror("mytar");
        exit(EXIT_FAILURE);
    }
    
    /* open just to read */
    if ((tarfile = open(filename, O_RDONLY)) == -1) {
        perror(filename);
        exit(EXIT_FAILURE);
    }
    
    /* we should only be reading in headers */
    while (read(tarfile, &head, BLOCK) > 0) {
        /* if end of archive is indicated, just return */
        if (check_currupt_archive(tarfile, &head, strict) == 0) {
            return;
        }
        
        /* so we can use next_header if needed next */
        size = get_size(&head);
        
        /* if no name is returned, just find the next header and start again */
        if ((name = get_name(&head, paths, npaths)) == NULL) {
            next_header(tarfile, size);
            continue;
        }
        
        /* non-verbose -> just print the name */
        if (!verbose) {
            printf("%s\n", name);      
        } else {
            /* grab all the info needed for the verbose list */
            mtime = get_mtime(&head);
            perms = get_perms(&head);
            owner = get_owner(&head);    
            
            printf("%10.10s %21.21s %8ld %16.16s %s\n",
                    perms, owner, size, mtime, name);
            free(mtime);
            free(perms);
            free(owner);
        }
        free(name);

        /* always move to the next header */
        next_header(tarfile, size);
    }
    close(tarfile);
}
