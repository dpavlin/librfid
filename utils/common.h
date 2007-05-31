#ifndef __COMMON_H__
#define __COMMON_H__

extern const char * hexdump(const void *data, unsigned int len);
extern int hexread(unsigned char *result, const unsigned char *in, unsigned int len);

#endif/*__COMMON_H__*/
