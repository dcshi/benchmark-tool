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
        sprintf(buf, "%s%s%s%s%s%s%s%s%s%s%s",
                "GET /lp?act_id=641023139&type=12&callback=crystal.callbackarea&rot=1 HTTP/1.1\r\n",
                "Host: l.qq.com\r\n",
                "Connection: keep-alive\r\n",
                "User-Agent: Mozilla/5.0 (Windows NT 5.1) AppleWebKit/535.11 (KHTML, like Gecko) Chrome/17.0.963.56 Safari/535.11\r\n",
                "Referer: http://lady.qq.com/\r\n",
                "Accept: */*\r\n",
                "Accept-Encoding: gzip,deflate,sdch\r\n",
                "Accept-Language: zh-CN,zh;q=0.8,en;q=0.6\r\n",
                "Accept-Charset: gb18030,utf-8;q=0.7,*;q=0.3\r\n",
                "Cookie: pgv_r_cookie=10122412984340; pvid=7110022893; notifyNewAppButtonDrag=1; qm_sid=9ab0cc19bb03590318d521cb2f583fe1,1QHJEOTRwbkYzQw..; qm_username=627474414; qm_domain=https://mail.qq.com; qm_qz_key=1_06b7e1dbeaa7e09d1cb9e5f63c11f605010a0b06060d00040708; pt2gguin=o627474414; show_id=; ptisp=ctc; qm_sk=627474414&iv-T7fN7; qm_ssum=627474414&8032dd8c28c419d68fe8f92181325c8a; lv_play_index=40; pgv_pvid=3163478505; pgv_info=ssid=s3063094676; o_cookie=627474414; luin=o0627474414; lskey=00010000c3c132dc325365cc920efd03d5624f38d75338fdfeaa39229d1e60bfeda11c8c4ce4a55e1185948e; uin=o0627474414; skey=@RmVRlXDex; snstoken=adad131sda\r\n",
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

