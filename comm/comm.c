/*
 *  written by Yubo
 */
#include <stdio.h>
#include "./comm.h"



void get_local_addr_comm(char *ip)
{
	FILE *fp;
	fp = fopen("../local_config", "r");
	if (fp == NULL)
	{
		printf("cannot open local config file\n");
		return -1;
	}
	char temp[24];
	fgets(temp, 24, fp);
	int i;
	for (i = 0; i < 24; ++i)
	{
		if ((temp[i] >= '0' && temp[i] <= '9') || temp[i] == '.')
		{
			ip[i] = temp[i];
		} else {
			break;
		}
	}
	ip[i] = '\0';
}

int get_cluster_info_comm(struct servers_comm *s_comm)
{
	FILE *fp;
	fp = fopen("../config", "r");
	if (fp == NULL)
	{
		printf("cannot open cluster config file\n");
		return -1;
	}

	int i = 0;
	char ip[17];
	while ( fgets(ip, 17, fp) )
	{
		if (ip[0] == '#')
			continue;

		int c;
		for (c = 0; i < 16; ++c)
		{
			if ((ip[c] >= '0' && ip[c] <= '9') || ip[c] == '.')
			{
				s_comm->server_list[i][c] = ip[c];
			} else {
				break;
			}
		}
		s_comm->server_list[i][c] = '\0';
		i++;
		if (i >= NODE_MAX)
		{
			printf("kv cluster overflow\n");
			return -1;
		}
	}
	fclose(fp);
	if (i == 0)
	{
		printf("read config error\n");
		return -1;
	}
	s_comm->servers_num = i;
	return 0;
}

// 0 is not local addr, 1 is local addr
int is_loacl_addr(char *l_addr, char *t_addr)
{
	int i;
	int l_len = strlen(l_addr);
	int t_len = strlen(t_addr);
	if (l_len != t_len)
		return 0;
	for (i = 0; i < l_len; ++i)
	{
		if (l_addr[i] != t_addr[i])
			return 0;
	}
	return 1;
}

int seri_mesg(struct remote_mesg *mesg, char *val)
{

}

int deseri_mesg(struct remote_mesg *mesg, char *val)
{

}

int setup_clients_comm(void)
{

}

int setup_servers_comm(struct servers_comm *s_comm)
{
	int ret;
	ret = get_cluster_info_comm(s_comm);
	if (ret != 0)
	{
		printf("setup server comm: get cluster config error\n");
		return -1;
	}

	// connect to other pacon servers
	char local_ip[17];
	get_local_addr_comm(local_ip);
	int i;
	int rc;
	for (i = 0; i < s_comm->servers_num; ++i)
	{
		if (is_loacl_addr(local_ip, s_comm->server_list[i]))
		{
			s_comm->server_mq[i] = NULL;
			s_comm->server_contx[i] = NULL;
			continue;
		}
		char ip[17];
		char bind_addr[64];
		int ip_len = strlen(ip);
		char *head = "tcp://";
		char *port = ":12580";
		strcpy(bind_addr, head);
		strcpy(bind_addr+strlen(head), s_comm->server_list[i]);
		strcpy(bind_addr+strlen(head)+strlen(s_comm->server_list[i]), port);
		void *context_cluster_rpc = zmq_ctx_new();
		int q_len = 0;
		rc = zmq_setsockopt(context_cluster_rpc, ZMQ_RCVHWM, &q_len, sizeof(q_len));
		void *cluster_rpc_req = zmq_socket(context_cluster_rpc, ZMQ_REQ);
		rc = zmq_connect(cluster_rpc_req, bind_addr);
		if (rc != 0)
		{
			printf("init cluster rpc error\n");
			return -1;
		}
		s_comm->server_mq[i] = cluster_rpc_req;
		s_comm->server_contx[i] = context_cluster_rpc;
		printf("bind cluster rpc: %s\n", bind_addr);
	}
}

int client_broadcast(struct clients_comm *c_comm, char *mesg)
{

}

int server_broadcast(struct servers_comm *s_comm, char *mesg)
{
	int ret;
	int i;
	for (i = 0; i < s_comm->servers_num; ++i)
	{
		if (s_comm->server_mq[i] != NULL)
		{
			char rep[2];
			zmq_send(s_comm->server_mq[i], mesg, strlen(mesg), 0);
			zmq_recv(s_comm->server_mq[i], rep, 1, 0);
			if (rep[0] != '0')
			{
				printf("broadcast to server %s fail\n", s_comm->server_list[i]);
				return -1;
			}
		}
	}
	return 0;
}



