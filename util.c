/*
 * file: util.c
 *
 * helper functions used by all of the modes
 */

#include <arpa/inet.h> 
#include <string.h>
#include <stdint.h>

#include "util.h"

/*
 * calculate the chksum field for the header
 */
int calculate_checksum(unsigned char *head) {
    int i, sum = 0;

    for (i = 0; i < sizeof(struct tarheader); i++) {
        /* not in the chksum field? add each byte to the sum */
        if (i < CHKSUM_BEGIN || i > CHKSUM_END) {
            sum += *(head+i);
        } else {
            /* skip the chksum field and just put in spaces*/
            sum += ' ';
        }
    }

    return sum;
}

/* given by assignment */
uint32_t extract_special_int(char *where, int len) {
    /* For interoperability with GNU tar. GNU seems to
    * set the high–order bit of the first byte, then
    * treat the rest of the field as a binary integer
    * in network byte order.
    * I don’t know for sure if it’s a 32 or 64–bit int, 
    * but for * this version, we’ll only support 32. (well, 31)
    * returns the integer on success, –1 on failure.
    * In spite of the name of htonl(), it converts int32 t */
    int32_t val = -1;
    if ( (len >= sizeof(val)) && (where[0] & 0x80)) {
        /* the top bit is set and we have space * extract the last four bytes */
        val = *(int32_t *)(where+len-sizeof(val));
        val = ntohl(val); /* convert to host byte order */
    }
    return val;
}

/* given by assignment */
int insert_special_int(char *where, size_t size, int32_t val) { 
    /* For interoperability with GNU tar. GNU seems to
    * set the high–order bit of the first byte, then
    * treat the rest of the field as a binary integer
    * in network byte order.
    * Insert the given integer into the given field
    * using this technique. Returns 0 on success, nonzero * otherwise
    */
    int err=0;
    if (val<0||(size<sizeof(val))){
        /* if it’s negative, bit 31 is set and we can’t use the flag
        * if len is too small, we can’t write it. * done.
        */
        err++;
    } else {
        /* game on....*/
        memset(where, 0, size);
        *(int32_t *)(where+size-sizeof(val)) = htonl(val); /* place the int */
        *where |= 0x80; /* set that high–order bit */
    }
    return err; 
}
