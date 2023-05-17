#ifndef _UTIL_H
#define _UTIL_H
#include <stdint.h>

#define BLOCK 512
#define RFLAG '0'
#define RFLAG_ALT '\0'
#define LFLAG '2'
#define DFLAG '5'

struct __attribute__ ((__packed__)) tarheader {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag[1];
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char pad[12]; /* make the struct 512 bytes */
};

int calculate_checksum(unsigned char *head);
int insert_special_int(char *where, size_t size, int32_t val);
uint32_t extract_special_int(char *where, int len);

#endif
