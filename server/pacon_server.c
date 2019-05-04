/*
 *  written by Yubo
 */

#include <stdio.h>
#include <zmq.h>
#include "pacon_server.h"


int start_pacon_server(struct *pacon_server_info)
{
	int ret;
	/*FILE *fp;
	fp = fopen("./server_config", "r");
	if (fp == NULL)
	{
		printf("cannot open server config file\n");
		return -1;
	}

	int i = 0;
	char ip[17];
	ret = fgets(ip, 17, fp);
	if (ret == 0)
	{
		printf("no value in server config file\n");
		return -1;
	} 

	int c;
	char mq_bind[28];
	for (c = 0; c < 28; ++c)
	{
		if (ip[c] != '\n' && ip[c] != '\0')
		{
			pacon_server_info->rec_mq_addr[c] = ip[c];
		} else {
			break;
		}
	}
	pacon_server_info->rec_mq_addr[c] = '\0';
	fclose(fp);*/

	// start mq
	printf("init zeromq\n");
	void *context = zmq_ctx_new();
	void *subscriber = zmq_socket(context, ZMQ_SUB);
	int rc = zmq_connect(subscriber, "ipc:///tmp/commit/0");
	if (rc != 0)
	{
		printf("init zeromq error\n");
		return -1;
	}
	pacon_server_info->subscriber = subscriber;
	pacon_server_info->context = context;
	return 0;
}

int stop_pacon_server(struct *pacon_server_info)
{
	int ret;
	zmq_close(pacon_server_info->subscriber);
	zmq_ctx_destroy(pacon_server_info->context);
	return 0;
}

int commit_to_fs(char *mesg)
{
	int ret;

}

int listen_mq(struct *pacon_server_info)
{
	int ret, ms_size;
	char mesg[255];
	while (1)
	{
		ms_size = zmq_recv(pacon_server_info->subscriber, mesg, 255, 0);
		if (ms_size == -1)
		{
			printf("error message\n");
			continue;
		}
		mesg[ms_size] = '\0';
		ret = commit_to_fs(mesg);
		if (ret != 0)
		{
			/* code */
		}
	}
}


int main(int argc, char const *argv[])
{
	int ret;
	printf("Strat Pacon Server\n");
	struct pacon_server_info *ps_info = (struct pacon_server_info *)malloc(sizeof(struct pacon_server_info));
	ret = start_pacon(ps_info);
	if (ret != 0)
	{
		printf("strat pacon error\n");
		return -1;
	}

	while (1)
	{

	}

	return 0;
}