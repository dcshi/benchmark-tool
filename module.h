#ifndef _MODULE_H_
#define _MODULE_H_

#include<stdlib.h>

/*
* encode success return the data len
* encode fail return -1
*/
int encodeRequest(char* data, int maxLen);

/*
* decode success return 0
* decode fail return -1
* not a complete package(tcp is stream), need wait more data return 1
*/
int decodeResponse(char* data, int len);

#endif
