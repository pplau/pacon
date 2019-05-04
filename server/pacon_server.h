/*
 *  written by Yubo
 */
#ifndef PACON_SERVER_H
#define PACON_SERVER_H

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

struct carea_info
{

};

struct pacon_server_info
{
	int carea_num;
	struct carea_info *carea_list;
	char rec_mq_addr[28];
	int rec_mq_port;
	void *subscriber;
	void *context;
};


int start_pacon_server(struct *pacon_server_info);

int stop_pacon_server(struct *pacon_server_info);

int commit_to_fs(char *mesg);

int new_carea(void);

int free_carea(void);

#endif