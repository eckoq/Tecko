#ifndef __tfc_md5__H
#define __tfc_md5__H

#include <sys/types.h>
#include <inttypes.h>

#include <string>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct {
  uint32_t state[4];        /* state (ABCD) */
  uint32_t count[2];        /* number of bits, modulo 2^64 (lsb first) */
  u_char buffer[64];         /* input buffer */
} MD5_CTX;

void MD5Init(MD5_CTX *);
void MD5Update(MD5_CTX *, unsigned char *, unsigned int);
void MD5Final(u_char [16], MD5_CTX *);

std::string MD5Output(char md5[16]);
char* md5_get_str(char md5[16]);

char* md5_str(u_char* buf,int size);
char* md5_buf(u_char* buf,int size);

char* md5_file(char* filename);

#ifdef __cplusplus
}
#endif

#endif
