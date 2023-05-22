/*
 * file: extract.c
 *
 * extract mode for mytar (argument x)
 *
 * If no names are given on the command line, mytar, 
 * extracts all the files in the archive
 *
 * If a name or names are given on the command line, 
 * mytar will extract the given path and any and all descendents of it
 *
 * Extract restores the modification time of the extracted files
 * (It should leave the access time alone)
 *
 */

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <utime.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "util.h"

/* used by extract to do utime after extraction is completed */
struct deferred_utime_operation {
    char* path;
    struct utimbuf newTime;
};

/* Function to create all necessary paths along a given path */
void check_dirs(char *path) {
    int i;
    char *temp_path;

    /* Define default perms */
    mode_t perms = S_IRWXU | S_IRWXG | S_IROTH;

    errno = 0;
    /* Allocate memory for a copy of the path */
    temp_path = (char *)malloc((strlen(path) + 1) * sizeof(char));
    /* If allocation fails, output an error and exit */
    if (temp_path == NULL) {
        perror("mytar");
        exit(EXIT_FAILURE);
    }

    /* Copy the path to the allocated memory */
    strcpy(temp_path, path);

    /* Iterate over the copy of the path */
    for (i = 0; temp_path[i]; i++) {
        /* If a directory separator is found */
        if (temp_path[i] == '/') {
            /* If it's the last character in the path, clean up and return */
            if (i >= strlen(path) - 1) {
                free(temp_path);
                return;
            }
            /* Temporarily end the string at the current directory level */
            temp_path[i] = '\0';
            /* Try to create the directory; 
             * it's not an error if it already exists */
            if (mkdir(temp_path, perms) && errno != EEXIST) {
                perror(path);
                exit(EXIT_FAILURE);
            }
            /* Restore the directory separator */
            temp_path[i] = '/';
        }
    }

    free(temp_path);
}

/* Function to extract file content from an archive */
void extract_file_content(int infile, int outfile, size_t file_size) {
    /* Buffer to hold file content */
    char *buff;
    
    /* Compute padding needed for the file content, based on BLOCK */
    size_t padding = file_size % BLOCK;

    /* Allocate memory for the buffer according to file size */
    buff = malloc(sizeof(char) * file_size);
    
    /* If memory allocation fails, output error and exit */
    if (buff == NULL) {
        perror("mytar");
        exit(EXIT_FAILURE);
    }

    /* Read file content from the archive into the buffer */
    if (read(infile, buff, file_size) == -1) {
        perror("mytar");
        exit(EXIT_FAILURE);
    }

    /* Write the read file content from the buffer to the output file */
    if (write(outfile, buff, file_size) == -1) {
        perror("mytar");
        exit(EXIT_FAILURE);
    }

    /* Skip padding bytes in the input file, to align with the BLOCK */
    if (lseek(infile, BLOCK - padding, SEEK_CUR) == -1) {
        perror("mytar");
        exit(EXIT_FAILURE);
    }

    free(buff);
}

/* Function to extract a regular file from an archive */
void extract_reg_file(int tarfile, const struct tarheader* header, char* path) {
    int new_file;
    mode_t perms;

    /* Convert file perms from octal string to mode_t */
    perms = (mode_t)strtol(header->mode, NULL, OCTAL);

    errno = 0;
    /* Open the new file with the given perms,
     * creating it if it doesn't exist and truncating it if it does */
    new_file = open(path, O_RDWR | O_CREAT | O_TRUNC, perms);
    if (new_file == -1) {
        perror(path);
        exit(EXIT_FAILURE);
    }

    /* Extract file content from the archive and write to the new file */
    extract_file_content(tarfile, new_file, strtol(header->size, NULL, OCTAL));

    close(new_file);
}


void extract_sym_link(int tarfile, struct tarheader* header, char* path) {
    /* Buffer to hold the target of the symbolic link */
    char* link;

    errno = 0;
    /* Allocate memory for the link buffer */
    link = calloc(LINK_MAX + 1, sizeof(char));
    if (link == NULL) {
        perror("mytar");
        exit(EXIT_FAILURE);
    }

    /* Copy the target of the symbolic link from the header to the buffer */
    strncpy(link, (char *)&header->linkname, LINK_MAX);

    errno = 0;
    /* Create the symbolic link */
    if (symlink(link, path) && errno != EEXIST) {
        perror(path);
        exit(errno);
    }

    free(link);
}


/* Function to extract a directory from an archive */
void extract_directory(int tarfile,const struct tarheader* header,char* path) {
    /* Variable to store perms for the new directory */
    mode_t perms = (mode_t)strtol(header->mode, NULL, OCTAL);

    errno = 0;
    /* Create the new directory with the given perms */
    if (mkdir(path, perms) && errno != EEXIST) {
        perror(path);
        exit(EXIT_FAILURE);
    }
}

/* Function to extract files from a tar archive */
void extract(char *filename,char **paths,int npaths, int verbose, int strict) {
    int tarfile;
    struct tarheader head;
    int i;
    unsigned long int fileSize;
    unsigned char typeFlag;
    char* path;
    char* pathNoLead;
    int found;
    int pathLength;

    /* Variables to hold file stats and new access/modification times */
    struct stat statBuffer;
    struct utimbuf newTime;

    /* List to hold deferred utime operations */
    struct deferred_utime_operation** deferred_ops = NULL;
    struct deferred_utime_operation* op;
    int deferred_ops_count = 0;

    /* Open the tar archive */
    tarfile = open(filename, O_RDONLY);
    if (tarfile == -1) {
        perror(filename);
        exit(EXIT_FAILURE);
    }


    /* Read from the tar archive until there's nothing left to read */
    while ((read(tarfile, &head, BLOCK)) > 0){
        /* if end of archive is indicated, just return */
        if (check_currupt_archive(tarfile, &head, strict) == 0) {
            return;
        }
        
        typeFlag = head.typeflag[0];

        /* Convert file size from octal string to unsigned long int */
        fileSize = strtol(head.size, NULL, OCTAL);

        /* Allocate memory for the file path + space for ./ */
        path = calloc(NAME_MAX_ + PREFIX_MAX + 3, sizeof(char));
        if (path == NULL) {
            perror("mytar");
            exit(EXIT_FAILURE);
        }

        /* Build the file path from the tar header */
        strcat(path, "./");
        if (head.prefix[0]) {
            strncat(path, (char*)&head.prefix, PREFIX_MAX);
            strcat(path, "/");
        }
        strncat(path, (char*)&head.name, NAME_MAX_);

        /* Remove leading "./" from the path */
        pathNoLead = path + 2;

        /* If paths are specified, 
         * check if the file is in one of the paths */
        if (paths) {
            found = 0;
            for (i = 0; i < npaths; i++) {
                pathLength = strlen(paths[i]);
                if (strncmp(pathNoLead, paths[i], pathLength) == 0) {
                    found = 1;
                    break;
                }
            }
            /* If the file is not in the paths, skip to the next header */
            if (!found) {
                if (fileSize > 0) {
                    if (lseek(tarfile, (fileSize / BLOCK + 1) * BLOCK,
                              SEEK_CUR) == -1) {
                        perror("mytar");
                        exit(errno);
                    }
                }
                free(path);
                continue;
            }
        }

        /* If verbose mode is on, print the file path */
        if (verbose) {
            printf("%s", path);
        }

        /* Ensure that the dirs in the path exist */
        check_dirs(pathNoLead);

        /* Extract the file based on its type */
        switch (typeFlag) {
            case RFLAG_ALT:
            case RFLAG:
                extract_reg_file(tarfile, &head, path);
                break; 
            case DFLAG:
                extract_directory(tarfile, &head, path);
                break;
            case LFLAG:
                extract_sym_link(tarfile, &head, path);
                break;

            default:
                fprintf(stderr, "mytar: invalid typeflag - '%c'", typeFlag);
                exit(EXIT_FAILURE);
        }

        /* Get file info */
        if (lstat(path, &statBuffer)) {
            perror("path");
            exit(errno);
        }

        /* Set new access time to the old one and
         * new modification time to the one from the tar header */
        newTime.actime = statBuffer.st_atime;
        newTime.modtime = strtol(head.mtime, NULL, OCTAL);

        /* Instead of calling utime() immediately after extraction, 
         * add a new deferred operation to the list. */
        op = malloc(sizeof(struct deferred_utime_operation));
        if (op == NULL) {
            perror("mytar");
            exit(EXIT_FAILURE);
        }
        op->path = strdup(path);
        if (op->path == NULL) {
            perror("mytar");
            exit(EXIT_FAILURE);
        }
        op->newTime = newTime;

        deferred_ops = realloc(deferred_ops, (deferred_ops_count + 1) *
                                                        sizeof(*deferred_ops));
        if (deferred_ops == NULL) {
            perror("mytar");
            exit(EXIT_FAILURE);
        }
        deferred_ops[deferred_ops_count] = op;
        deferred_ops_count++;

        /* Free up the memory used by the file path */
        free(path);
    }

    /* Now that all files and symbolic links have been created, 
     * perform the deferred utime operations */
    for (i = 0; i < deferred_ops_count; i++) {
        if (utime(deferred_ops[i]->path, &(deferred_ops[i]->newTime))) {
            perror("mytar");
            exit(EXIT_FAILURE);
        }
        free(deferred_ops[i]->path);
        free(deferred_ops[i]);
    }
    free(deferred_ops);

    close(tarfile);
}
