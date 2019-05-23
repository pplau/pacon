/*
 *  written by Yubo
 */
#ifndef COMM_H
#define COMM_H

#include <zmq.h>

#define NODE_MAX 64

#define SYNC_FOR_RM 1
#define SYNC_FOR_READDIR 2
#define SYNC_FOR_LINK 3
#define SYNC_FOR_UNLINK 4


struct remote_mesg
{
	uint32_t ca_id;   // consistent area id
	uint32_t node_id;  // 
	uint32_t opt;
};

struct clients_comm
{
	int client_num;

};

struct servers_comm
{
	int servers_num;
	char server_list[NODE_MAX][17];
	void* server_mq[NODE_MAX];
	void* server_contx[NODE_MAX];
};


int seri_mesg(struct remote_mesg *mesg, char *val);

int deseri_mesg(struct remote_mesg *mesg, char *val);

int setup_clients_comm(void);

int setup_servers_comm(struct servers_comm *s_comm);

int client_broadcast_sync(struct clients_comm *c_comm);

int server_broadcast_sync(struct servers_comm *s_comm);



#endif