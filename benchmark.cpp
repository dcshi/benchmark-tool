/* benchmark utility. 
 *
 * Copyright (c) 2011-2012, dcshi <dcshi at qq dot com>
 * All rights reserved.
 *
 * benchmark [-h <host>] [-p <port>] [-c <clients>] [-n <requests]> [-k] [-r] [-u]
 * -h <host> Server ip (default 127.0.0.1)
 * -p <port> Server port (default 5113)
 * -c <clients> Number of parallel connections (default 5)
 * -n <requests> Total number of requests (default 50)
 * -k keep alive or reconnect (default is reconnect)
 * -r re-encode sendbuf per request (default is no encode)
 * -u benchmark with udp protocol(default is tcp)
 */

#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<errno.h>
#include<stdint.h>
#include<signal.h>
#include<sys/time.h>
#include<iostream>
#include<sys/epoll.h>
#include<fcntl.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<netinet/tcp.h>
#include<arpa/inet.h>
#include <getopt.h>
#include "module.h"

#ifdef DEBUG
#include<profiler.h>
#endif

using namespace std;

#define MAX_RECV_BUF_LEN 4*1024
#define MAX_SEND_BUF_LEN 4*1024
#define MAX_FAIL_COUNT 1000

#define ENONE 0
#define READABLE 1
#define WRITEABLE 2

#define CONNECT_TIMEOUT 3

typedef enum 
{
	PROTOCOL_TCP = 1,
	PROTOCOL_UDP
}PROTOCOL;

typedef int funcProc(void* );
int createClient();
int sendToServer(void* );
int recvFromServer(void* );

typedef struct 
{
	int stop;
	int epfd;
	struct epoll_event* events;
}epollLoop;

typedef struct
{
	int fd;
	int mask;
	int64_t latency;
	uint64_t startTime;
    uint64_t touchTime;
	unsigned recvBufLen;
	unsigned sendedLen;
	unsigned sendBufLen;
	char recvBuf[MAX_RECV_BUF_LEN + 1]; 
	char sendBuf[MAX_SEND_BUF_LEN + 1]; 
	funcProc* rFuncProc;
	funcProc* wFuncProc;
}client;

typedef struct
{	
	//被重置的clients数目
	unsigned resetClientNum;
	//超时被清除的client数
	unsigned timeoutClientNum;
	//实际完成的请求数，包括超时重发
	unsigned totalRequests;
	//实际需要成功完成的请求数，-n 参数传入，默认是50
	unsigned needRequestNum;
	//已经完成的请求数，每次成功接受一个完成数据包，会+1
	unsigned doneRequests;
	//开始测试时间
	uint64_t startTime;
	//结束测试时间
	uint64_t endTime;
	//是否采用长连接模式，-k 参数表示采用长连
	int keepalive;
	//当前存活的client数(并发连接数)
	unsigned liveClientNum;
	//实际创建的client次数
	unsigned createClientNum;
	//需要的并发连接数 -c参数
	unsigned needClientNum;
    //是否随机发送内容
    int randomFlag;
	unsigned short port;
	unsigned short protocol; //1 for tcp(default),2 for udp
	const char* host;
	//为每个请求耗时记录，用户最后统计不同耗时区间的请求数
	uint64_t* latency;
	epollLoop* el;
	client* clients;
}benchConfig;

benchConfig config;

void ignoreSignal()
{
	signal(SIGTSTP,SIG_IGN); 
	signal(SIGHUP, SIG_IGN);
	signal(SIGQUIT,SIG_IGN);
	signal(SIGPIPE,SIG_IGN);
	signal(SIGTTOU,SIG_IGN);
	signal(SIGTTIN,SIG_IGN);
	signal(SIGSTOP,SIG_IGN);
	signal(SIGTERM,SIG_IGN);
	signal(SIGINT, SIG_IGN);
	signal(SIGTSTP,SIG_IGN);
}

static struct timeval _tv = {0, 0};

uint64_t msTime()
{
	uint64_t msec;

    if (_tv.tv_sec == 0)
	    gettimeofday(&_tv, NULL);

	msec = ((long)_tv.tv_sec*1000LL);
	msec += _tv.tv_usec/1000;

	return msec;
}

uint64_t usTime()
{
	uint64_t usec;

    if (_tv.tv_sec == 0)
	    gettimeofday(&_tv, NULL);

	usec = ((uint64_t)_tv.tv_sec)*1000000LL;
	usec += _tv.tv_usec;

	return usec;
}

void touchTime()
{
    gettimeofday(&_tv, NULL);
}

void initConfig()
{
	config.createClientNum = 0;
	config.timeoutClientNum = 0;
	config.totalRequests = 0;
	config.needRequestNum = 50;
	config.doneRequests = 0;
	config.port = 5113;
	config.startTime = 0;
	config.endTime = 0;
	config.keepalive = 0;
	config.liveClientNum = 0;
	config.needClientNum = 5;
    config.randomFlag = 0;
	config.host = "127.0.0.1";
	config.protocol = PROTOCOL_TCP;
	config.latency = NULL;
	config.el = NULL;
	config.clients = NULL;
}

struct option longopts[] = {
	{ "concurrency", required_argument, NULL, 'c' },
	{ "requests", required_argument, NULL, 'n' },
	{ "host", required_argument, NULL, 'h' },
	{ "port", required_argument, NULL, 'p' },
	{ "keepalive", no_argument, NULL, 'k' },
	{ "random", no_argument, NULL, 'r' },
	{ "protocol", no_argument, NULL, 'u' },
	//{ "help", no_argument, NULL, 'h' },
	{ 0, 0, 0, 0 },
};

void parseOptions(int argc, char **argv) {
	int opt = 0;
	while ((opt = getopt_long(argc, argv, ":c:n:h:p:kru", longopts, NULL)) != -1)
	{
		switch (opt) {
			case 'c':
				config.needClientNum = atoi(optarg);
				break;
			case 'n':
				config.needRequestNum = atoi(optarg);
				break;
			case 'h':
				config.host = optarg;
				break;
			case 'p':
				config.port = atoi(optarg);
				break;
			case 'k':
				config.keepalive = 1;
				break;
			case 'u':
				config.protocol = PROTOCOL_UDP;	
				break;
            case 'r':
                config.randomFlag = 1;
                break;
			default:
				cout<<"Usage: benchmark [-h <host>] [-p <port>] [-c <clients>] [-n <requests]> [-k] [-r] [-u]"<<endl;
				cout<<" -h <host> Server ip (default 127.0.0.1)"<<endl;
				cout<<" -p <port> Server port (default 5113)"<<endl;
				cout<<" -c <clients> Number of parallel connections (default 5)"<<endl;
				cout<<" -n <requests> Total number of requests (default 50)"<<endl;
				cout<<" -k keep alive or reconnect (default is reconnect)"<<endl;
				cout<<" -r re-encode sendbuf per request (default is no encode)"<<endl;
				cout<<" -u benchmark with udp protocol(default is tcp)"<<endl;
				exit(1);
			}
	}
}

epollLoop* createEpollLoop(int maxClients)
{
	epollLoop *el;
	el = (epollLoop*)malloc(sizeof(*el));
	if(!el)
	  return NULL;

	el->epfd = epoll_create(10240);
	el->stop = 1;
	el->events = new epoll_event[maxClients];

	return el;
}

int addEvent(client* c, int mask, funcProc* func, benchConfig* conf)
{
	struct epoll_event e;
	int op = c->mask == ENONE ? EPOLL_CTL_ADD : EPOLL_CTL_MOD; 
	e.events = 0;
	e.data.u64 = 0;
	e.data.fd = c->fd; 
	c->mask |= mask; /* Merge old events */

	if(c->mask & READABLE)
	{   
		e.events |= EPOLLIN;

	}   
	if(c->mask & WRITEABLE)
	{   
		e.events |= EPOLLOUT;
	}   

	if(mask & WRITEABLE)
	{   
		c->wFuncProc = func;
	}   
	else
	{   
		c->rFuncProc = func;
	}   

	return epoll_ctl(conf->el->epfd, op, c->fd, &e);
}

int delEvent(client* c, int mask, benchConfig* conf)
{
	c->mask = c->mask & (~mask);
	struct epoll_event e;
	e.events = 0;
	e.data.u64 = 0; /* avoid valgrind warning */
	e.data.fd = c->fd;

	if (c->mask & READABLE) 
	{
		e.events |= EPOLLIN;
	}
	if (c->mask & WRITEABLE)
	{
		e.events |= EPOLLOUT;
	}

	int op = c->mask == ENONE ? EPOLL_CTL_DEL : EPOLL_CTL_MOD;

	return epoll_ctl(conf->el->epfd, op, c->fd, &e);
}

void closeConnection(benchConfig* conf, client* c)
{
	struct epoll_event e;
	e.events = 0;
	epoll_ctl(conf->el->epfd, EPOLL_CTL_DEL, c->fd, &e);

	close(c->fd);
	c->fd = -1;

	conf->liveClientNum--;
}

void resetClient(client* c)
{
	//delEvent(c, WRITEABLE, &config);
	delEvent(c, READABLE, &config);
	addEvent(c, WRITEABLE, sendToServer, &config);
	
	c->recvBufLen = 0;
	c->sendedLen = 0;

    if (config.randomFlag == 1)
    {
        c->sendBufLen = encodeRequest(c->sendBuf, sizeof(c->sendBuf)) ;
        if (c->sendBufLen < 0)
        {
            config.liveClientNum--;
            close(c->fd);
            return ;
        }
    }

	config.resetClientNum++;
}

void clientDone(client* c)
{
	if(config.doneRequests == config.needRequestNum)
	{
		config.el->stop = 0;
		return;
	}

	//there is no need to re-create a udp socket
	if(config.keepalive || config.protocol == PROTOCOL_UDP)
	{
		resetClient(c);
	}
	else
	{
		closeConnection(&config, c);
		createClient();
	}
}

int recvFromServer(void* cl)
{
	client* c = (client*)cl;
    c->touchTime = usTime();

    /* Calculate latency only for the first read event. This means that the 
     * server already sent the reply and we need to parse it. Parsing overhead                                       
     * is not part of the latency, so calculate it only once, here. */
	if(c->latency < 0)
		c->latency = usTime() - c->startTime;

	//Calculate from the first response 
	//if (config.startTime == 0)
	//	config.startTime = msTime();

	while(1)
	{
		int received = read(c->fd, c->recvBuf + c->recvBufLen, sizeof(c->recvBuf) - c->recvBufLen);
		if(received < 0)
		{
			if(errno == EINTR)
			{
				continue;
			}   
			else if(errno == EWOULDBLOCK || errno == EAGAIN)
			{
				return 1;
			}
			else
			{
				closeConnection(&config, c);
				createClient();
				return -1;
			}
		}
		else if(received == 0)
		{
			//peer closed
			closeConnection(&config, c);
			createClient();
			return -1;
		}
		else
		{
			c->recvBufLen += received;
			break;
		}
	}

    //sizeof(c->recvBuf) = MAX_RECV_BUF_LEN + 1
    if (c->recvBufLen > MAX_RECV_BUF_LEN) {
        cout<<"client recv buffer is full, Please make sure that is enough"<<endl;
        exit(-1);
    }

	//encode request package
	int ret = decodeResponse(c->recvBuf, c->recvBufLen);
	if(ret < 0)
	{
		closeConnection(&config, c);
		createClient();
		return -1;
	}
	else if(ret == 1)
	{
		//it is not a complete packet, wait it
		return 0;
	}

	if(config.doneRequests < config.needRequestNum)
	{
		config.latency[config.doneRequests++] = c->latency;
	}

	clientDone(c);
	return 0;
}

int sendToServer(void* cl)
{
	client* c = (client*)cl;

    //first send
    if (c->sendedLen == 0)
    {
	    c->startTime = usTime();
        c->latency = -1;
    }
    c->touchTime = usTime();

	int sended;
	while(1)
	{   
		sended = write(c->fd, c->sendBuf + c->sendedLen, c->sendBufLen - c->sendedLen);
		if(sended <= 0 ) 
		{   
			if (errno == EINTR)
			{   
				continue;
			}   
			else if(errno == EWOULDBLOCK || errno == EAGAIN)
			{
				return 1;
			}
			else
			{
				closeConnection(&config, c);
				createClient();
				return -1;
			}
		}   
		break;
	}

	config.totalRequests++;
	c->sendedLen += sended;
	if(c->sendedLen == c->sendBufLen)
	{  
		delEvent(c, WRITEABLE, &config);
		addEvent(c, READABLE, recvFromServer, &config);
	}   

	return 0;
}

int setNonBlock(int fd)
{
	int flag;

	if((flag = fcntl(fd, F_GETFL, 0)) == -1)
		return -1;

	flag |= O_NONBLOCK;

	if(fcntl(fd, F_SETFL, flag) == -1)
		return -1;

	return 0;

}

int setTcpNoDelay(int fd)
{
	int yes = 1;

	if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes)) == -1) 
		return -1;

	return 0;
}

int createClient()
{
	int fd;
    client *c;

	if(config.protocol == PROTOCOL_UDP)
		fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	else
		fd = socket(AF_INET, SOCK_STREAM, 0);

	if(fd == -1)
    {
        cout<<"CreateClient:create sokcet fail"<<endl;
		return -1;
    }

	//set noblock
	if(setNonBlock(fd) < 0 )
        goto CREATE_FAIL;

	struct sockaddr_in addr;
	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET; 
	addr.sin_port = htons(config.port);
	addr.sin_addr.s_addr= inet_addr(config.host);

	if(connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
	{
		/*this is ok*/
		if(errno != EINPROGRESS)
		{
			cout<<"CreateClient:connect to Server Fail"<<endl;
            goto CREATE_FAIL;
		}
	}

	if(config.protocol == PROTOCOL_TCP && setTcpNoDelay(fd) < 0)
        goto CREATE_FAIL;

	c = config.clients + fd % config.needClientNum;
	c->fd = fd;
	c->mask = ENONE;
	
	c->sendBufLen = encodeRequest(c->sendBuf, sizeof(c->sendBuf));
    if (c->sendBufLen < 0) 
	{
		cout<<"CreateClient:encode request buffer fail"<<endl;
        goto CREATE_FAIL;
	}

    c->sendedLen = 0;
    c->recvBufLen = 0;

	if(addEvent(c, WRITEABLE, sendToServer, &config) < 0)
        goto CREATE_FAIL;

	config.liveClientNum++;	
	config.createClientNum++;

	return 0;

CREATE_FAIL:
    cout<<"CreateClient fail"<<endl;
    close(fd);
    return -1;
}

void createNeedClients()
{
	while(config.liveClientNum < config.needClientNum)
	{
		if(createClient() < 0)
		{
			cout<<"create client faild"<<endl;
			exit(1);
		}
	}
}

int compareLatency(const void *a, const void *b) 
{
	return (*(uint64_t*)a)-(*(uint64_t*)b);
}

void showThroughPut()
{
    uint64_t totalTime = msTime() - config.startTime;
    float rps = (float)config.doneRequests / ((float)totalTime/1000);
    
    cout<<"total time "<<totalTime<<"(ms) "<<config.doneRequests<<"requests done => rps : "<<rps<<endl;
}

void showReport()
{
	uint64_t totalTime = config.endTime - config.startTime;
	float reqpersec = (float)config.doneRequests / ((float)totalTime/1000);

	if(config.keepalive == 1)
	{
		cout<<"keepAlive is open"<<endl;
	}
	cout<<config.needClientNum<<" parallel clients"<<endl;
	cout<<config.doneRequests<<" completed in "<<totalTime/1000<<" seconds"<<endl;

	unsigned i, curlat = 0;
	uint64_t total_lat = 0;
	uint64_t avg_lat = 0;
	float perc;
	qsort(config.latency, config.doneRequests, sizeof(uint64_t), compareLatency);

	for (i = 0; i < config.doneRequests; i++) {
		
		total_lat += config.latency[i]/1000LL;

		if (config.latency[i]/10000 != curlat || i == (config.doneRequests-1)) {
			curlat = config.latency[i]/10000;
			perc = ((float)(i+1)*100)/config.doneRequests;
			cout<<perc<<"% <= "<<config.latency[i]/1000<<" milliseconds"<<endl;
		}
	}
	avg_lat = total_lat/config.doneRequests;

	cout<<avg_lat<<" ms average latency"<<endl;
	cout<<config.needRequestNum<<" requests are neend"<<endl;
	cout<<config.totalRequests<<" requests are sended"<<endl;
	cout<<config.timeoutClientNum<< " clients are timeout"<<endl;
	cout<<config.resetClientNum<<" clients are reset"<<endl;
	cout<<config.createClientNum<<" clients are created"<<endl;
	cout<<(config.endTime - config.startTime)<<" total milliseconds used"<<endl;
	cout<<reqpersec<<" requests per second"<<endl;
}

void clearTimeoutConnection(benchConfig* conf, uint64_t curTime)
{
	uint64_t timeout = CONNECT_TIMEOUT*1000000;
	for(unsigned i = 0; i< conf->needClientNum; i++)
	{
		client* c = conf->clients + i;
		if(c->fd > 0 && (c->touchTime + timeout) <= curTime)
		{
			conf->timeoutClientNum++;
			resetClient(c);
		}
	}
}

void benchmark()
{
	int epoll_fail = 0;
	//create clients we need
	createNeedClients();

	config.startTime = msTime();
    uint64_t last_check_ustime = usTime();

	while(config.el->stop)
	{
		int num;
		num = epoll_wait(config.el->epfd, config.el->events, config.needClientNum+1, 2000);

        //update global time per loop
        touchTime();

		for(int i = 0 ;i< num; i++)
		{
			struct epoll_event *e = config.el->events + i;
			client* c = config.clients + e->data.fd % config.needClientNum;

			if(e->events & (EPOLLERR | EPOLLHUP))
			{
				if(++epoll_fail > MAX_FAIL_COUNT)
				{
					cout<<"the epoll fail count is more than "<<MAX_FAIL_COUNT<<endl;
					cout<<"the network is so poor,check it and try again."<<endl;
					exit(0);
				}
				closeConnection(&config, c);
				createClient();			
				continue;
			}
			if(e->events & EPOLLIN)
			{
				c->rFuncProc(c);
			}
			if(e->events & EPOLLOUT)
			{
				c->wFuncProc(c);
			}
		}

		//check timeout connection(check per 200ms => 200000us)
        if ((usTime() - last_check_ustime) > 200000)
        {
        	clearTimeoutConnection(&config, usTime());            
			createNeedClients();

            //Non-essential, and it also effect performance
            showThroughPut();
    
            last_check_ustime = usTime();
        }

	}

	config.endTime = msTime();
	showReport();
}


int main(int argc, char** argv)
{
	//ignoreSignal();
	
	initConfig();
	
    parseOptions(argc, argv);

	config.clients = new client[config.needClientNum];
	config.latency = new uint64_t[config.needRequestNum];

	config.el = createEpollLoop(config.needClientNum+1);

	benchmark();

	return 0;
}
