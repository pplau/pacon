/*
 *  written by Yubo
 */

#include <stdio.h>
#include <string.h>
#include <zmq.h>
#include "pacon_server.h"

#define PATH_MAX 128

// opt type
#define MKDIR ":1"
#define CREATE ":2"
#define RM ":3"
#define RMDIR ":4"
#define LINK ":5"


int start_pacon_server(struct pacon_server_info *ps_info)
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
			ps_info->rec_mq_addr[c] = ip[c];
		} else {
			break;
		}
	}
	ps_info->rec_mq_addr[c] = '\0';
	fclose(fp);*/

	// start mq
	printf("init zeromq\n");
	void *context = zmq_ctx_new();
	void *subscriber = zmq_socket(context, ZMQ_SUB);
	int rc = zmq_connect(subscriber, "ipc:///tmp/pacon_commit");
	if (rc != 0)
	{
		printf("init zeromq error\n");
		return -1;
	}
	ps_info->subscriber = subscriber;
	ps_info->context = context;

	ps_info->batch_dir_mode = S_IFDIR | 0755;
	ps_info->batch_file_mode = S_IFREG | 0644;
	return 0;
}

int stop_pacon_server(struct pacon_server_info *ps_info)
{
	int ret;
	zmq_close(ps_info->subscriber);
	zmq_ctx_destroy(ps_info->context);
	return 0;
}

int commit_to_fs(struct pacon_server_info *ps_info, char *mesg)
{
	int ret = -1;
	int mesg_len = strlen(mesg);
	char path[PATH_MAX];
	strncpy(path, mesg, mesg_len-2);
	path[mesg_len] = '\0';

	switch (mesg[mesg_len-1])
	{
		case '1':
			printf("coomit to fs, typs: MKDIR\n");
			//ret = mkdir(path, ps_info->batch_dir_mode);
			break;

		case '2':
			printf("coomit to fs, typs: CREATE\n");
			//ret = create(path, ps_info->batch_file_mode);
			break;

		case '3':
			break;

		case '4':
			break;

		case '5':
			break;

		default:
			printf("opt type error\n");
			return -1;
	}
	return ret;
}

int listen_mq(struct pacon_server_info *ps_info)
{
	int ret, ms_size;
	char mesg[PATH_MAX];
	while (1)
	{
		ms_size = zmq_recv(ps_info->subscriber, mesg, PATH_MAX, 0);
		if (ms_size == -1)
		{
			printf("error message\n");
			continue;
		}
		mesg[ms_size] = '\0';
		ret = commit_to_fs(ps_info, mesg);
		if (ret != 0)
		{
			printf("commit opt to fs fail: %s\n", mesg);
			return -1;
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

	ret = listen_mq(ps_info);
	if (ret != 0)
	{
		printf("listen mq error\n");
		return -1;
	}

	return 0;
}