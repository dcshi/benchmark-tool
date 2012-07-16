#include "../module.h"

#include <iostream>
#include <stdio.h>
#include <string.h>

using namespace std;

static char buf[4096];
char *ptr = NULL;


int encodeRequest(char* data, int maxLen)
{
    //只做一次初始化
    if (ptr == NULL)
    {
        sprintf(buf, "%s%s%s%s%s%s%s%s%s%s",
                "GET /lp?id=641023139&type=12&callback=callbackarea&rot=1 HTTP/1.1\r\n",
                "Host: xx.com\r\n",
                "Connection: keep-alive\r\n",
                "User-Agent: Mozilla/5.0 (Windows NT 5.1) AppleWebKit/535.11 (KHTML, like Gecko) Chrome/17.0.963.56 Safari/535.11\r\n",
                "Referer: http://xx.com/\r\n",
                "Accept: */*\r\n",
                "Accept-Encoding: gzip,deflate,sdch\r\n",
                "Accept-Language: zh-CN,zh;q=0.8,en;q=0.6\r\n",
                "Accept-Charset: gb18030,utf-8;q=0.7,*;q=0.3\r\n",
                "\r\n");
        ptr = buf;
    }

    int len = strlen(ptr);
       
    if (len > maxLen) 
        return -1;

    memcpy(data, ptr, len);

	return len;
}

int decodeResponse(char* data, int len)
{
	return 0;
}

