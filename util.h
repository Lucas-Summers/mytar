#ifndef _UTIL_H
#define _UTIL_H
#include <stdint.h>

#define RFLAG '0'
#define RFLAG_ALT '\0'
#define LFLAG '2'
#define DFLAG '5'

#define PATH_MAX_ 256
#define NAME_MAX_ 100
#define UGNAME_MAX 32
#define ID_MAX 07777777
#define SIZE_MAX_ 077777777777
#define MTIME_MAX 077777777777
#define LINK_MAX 100

#define BLOCK 512
#define MTIME_SIZE 12
#define SIZE_SIZE 12
#define ID_SIZE 8
#define PERM_STRLEN 10
#define MTIME_STRLEN 16
#define OWNER_STRLEN 64
#define VERSION_SIZE 2
#define MAGIC_SIZE 5

#define EMPTY_CHKSUM 256
#define CHKSUM_BEGIN 148
#define CHKSUM_END 155
#define OCTAL 8


#define MODE_MASK 07777
#define PERM_MASK 256
#define SPECIAL_INT_MASK 0x800000

/* all fields are made chars so we dont get warnings when using
 * functions like sprintf which expect a char
 */
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
    char pad[12]; /* make the struct perfectly 512 bytes */
};

int calculate_checksum(unsigned char *head);
int insert_special_int(char *where, size_t size, int32_t val);
uint32_t extract_special_int(char *where, int len);

#endif
