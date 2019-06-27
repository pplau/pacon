/*
 *  written by Yubo
 */

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <zmq.h>
#include "pacon_server.h"

#define PATH_MAX 512
#define TIMESTAMP_SIZE 11

// opt type
#define MKDIR ":1"
#define CREATE ":2"
#define RM ":3"
#define RMDIR ":4"
#define READDIR ":5"
#define OWRITE ":6"  // data size is larger than the INLINE_MAX, write it back to DFS
#define FSYNC ":7"
#define BARRIER ":8"
#define DEL_BARRIER ":9"
#define CHECKPOINT ":0"

static uint32_t commit_barrier = 0;  // 0 is not barrier, != 0 is timestamp
static int reach_barrier = 0;        // 0 is not barrier
									 // 1 is local rpc receives a barrier but commit mq doesn't reach barrier
									 // 2 is commit mq already reached to the barrier
static int remote_reach_barrier = 0;
static int mesg_count = 0; 


enum statflags
{
	STAT_type,  // 0: dir, 1: file
	STAT_inline,  // 0: no inline data, 1: has inlien
	STAT_commit,  // 0: not yet be committed, 1: already be committed
	STAT_file_created,  // 0: not be created in DFS, 1: already be created in DFS
	STAT_existedin_dfs,  //  0: a new item after pacon run, 1: it was created before pacon run
	STAT_child_check,  //  0: haven't check its children, 1: already checked its children
	STAT_rm,
};


void server_set_stat_flag(struct pacon_stat_server *p_st, int flag_type, int val)
{
	if (val == 0)
	{
		// set 0
		p_st->flags &= ~(1UL << flag_type);
	} else {
		// set 1
		p_st->flags |= 1UL << flag_type;
	}
}

int server_get_stat_flag(struct pacon_stat_server *p_st, int flag_type)
{
	if (((p_st->flags >> flag_type) & 1) == 1 )
	{
		return 1;
	} else {
		return 0;
	}
}

// seri pacom_stat and inline_data to val
int server_seri_inline_data(struct pacon_stat_server *s, char *inline_data, char *val)
{
	//int size = sizeof(struct pacon_stat);
	if (s != NULL)
	{
		memcpy(val, s, PSTAT_SIZE);
	}
	memcpy(val + PSTAT_SIZE, inline_data, strlen(inline_data)+1);
	//memcpy(val + PSTAT_SIZE, inline_data, strlen(inline_data));
	return PSTAT_SIZE + INLINE_MAX;
}

// deseri val to pacon_stat and inline_data
int server_deseri_inline_data(struct pacon_stat_server *s, char *inline_data, char *val)
{
	//int size = sizeof(struct pacon_stat);
	if (s != NULL)
	{
		memcpy(s, val, PSTAT_SIZE);
	}
	strcpy(inline_data, val + PSTAT_SIZE);
	//memcpy(inline_data, val + PSTAT_SIZE, strlen(val) - PSTAT_SIZE);
	return PSTAT_SIZE + INLINE_MAX;
}

void get_local_addr(char *ip)
{
	FILE *fp;
	fp = fopen("../local_config", "r");
	if (fp == NULL)
	{
		printf("cannot open config file\n");
		return -1;
	}
	char temp[17];
	fgets(temp, 17, fp);
	int i;
	for (i = 0; i < 16; ++i)
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

int start_pacon_server(struct pacon_server_info *ps_info)
{
	int ret;
	int rc;

	// init kv
	struct dmkv *kv = (struct dmkv *)malloc(sizeof(struct dmkv));
	set_dmkv_config_type(1);
	ret = dmkv_init(kv);
	if (ret != 0)
	{
		printf("init dmkv fail\n");
		return -1;
	}
	ps_info->kv_handle = kv;

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

	// start coomit mq
	printf("init commit mq\n");
	void *context = zmq_ctx_new();
	void *subscriber = zmq_socket(context, ZMQ_SUB);
	char *filter = "";
	int q_len = 0;
	rc = zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, filter, strlen (filter));
	rc = zmq_setsockopt(subscriber, ZMQ_RCVHWM, &q_len, sizeof(q_len));
	rc = zmq_bind(subscriber, "ipc:///run/pacon_commit");
	if (rc != 0)
	{
		printf("init zeromq error\n");
		return -1;
	}
	ps_info->subscriber = subscriber;
	ps_info->context = context;

	// start local rpc mq
	printf("init rpc mq\n");
	void *context_local_rpc = zmq_ctx_new();
	void *local_rpc_rep = zmq_socket(context_local_rpc, ZMQ_REP);
	rc = zmq_setsockopt(local_rpc_rep, ZMQ_RCVHWM, &q_len, sizeof(q_len));
	rc = zmq_bind(local_rpc_rep, "tcp://127.0.0.1:5555");
	if (rc != 0)
	{
		printf("init local rpc error\n");
		return -1;
	}
	ps_info->local_rpc_rep = local_rpc_rep;
	ps_info->context_local_rpc = context_local_rpc;

	// start cluster rpc mq
	printf("init cluster rpc mq\n");
	char local_ip[17];
	get_local_addr(local_ip);
	char bind_addr[64];
	int ip_len = strlen(local_ip);
	char *head = "tcp://";
	char *port = ":12580";
	strcpy(bind_addr, head);
	strcpy(bind_addr+strlen(head), local_ip);
	strcpy(bind_addr+strlen(head)+strlen(local_ip), port);

	void *context_cluster_rpc = zmq_ctx_new();
	void *cluster_rpc_rep = zmq_socket(context_cluster_rpc, ZMQ_REP);
	rc = zmq_setsockopt(context_cluster_rpc, ZMQ_RCVHWM, &q_len, sizeof(q_len));
	rc = zmq_bind(cluster_rpc_rep, bind_addr);
	if (rc != 0)
	{
		printf("init cluster rpc error\n");
		return -1;
	}
	ps_info->cluster_rpc_rep = cluster_rpc_rep;
	ps_info->context_cluster_rpc = context_cluster_rpc;

	// init server cluster info
	printf("init server cluster info\n");
	struct servers_comm *s_comm = (struct servers_comm *)malloc(sizeof(struct servers_comm));
	setup_servers_comm(s_comm);
	ps_info->s_comm = s_comm;
	
	ps_info->batch_dir_mode = S_IFDIR | 0755;
	ps_info->batch_file_mode = S_IFREG | 0644;
	return 0;
}

int stop_pacon_server(struct pacon_server_info *ps_info)
{
	int ret;
	zmq_close(ps_info->subscriber);
	zmq_ctx_destroy(ps_info->context);
	zmq_close(ps_info->local_rpc_rep);
	zmq_ctx_destroy(ps_info->context_local_rpc);
	zmq_close(ps_info->cluster_rpc_rep);
	zmq_ctx_destroy(ps_info->context_cluster_rpc);
	free(ps_info->s_comm);
	free(ps_info);
	return 0;
}

int retry_commit(struct pacon_server_info *ps_info, char *path, int type)
{
	int ret, i;
	for (i = 0; i < 5; ++i)
	{
		if (type == 1)
		{
			ret = mkdir(path, ps_info->batch_dir_mode);
			if (ret != -1)
				return 0;
		}
		if (type == 2)
		{
			ret = creat(path, ps_info->batch_dir_mode);
			if (ret != -1)
				return 0;
		}
		if (type == 3)
		{
			ret = remove(path);
			if (ret == 0)
				return 0;
		}
	}

	// try to commit in global sync way
	return -1;
}

int commit_to_fs(struct pacon_server_info *ps_info, char *mesg)
{
	int ret = -1;
	int fd; 
	int mesg_len = strlen(mesg);
	char path[PATH_MAX];
	//strncpy(path, mesg, mesg_len-2);
	//path[mesg_len-2] = '\0';
	int i;
	for (i = 0; i < mesg_len; ++i)
	{
		if (mesg[i] != ':')
			path[i] = mesg[i];
		else
			break;
	}
	path[i] = '\0';

	// only the remote barrier will go into this logic
	if (commit_barrier != 0)
	{
		// get timestamp
		uint32_t timestamp;
		timestamp = atoi(mesg+i+2);
		if (timestamp > commit_barrier)
		{
			remote_reach_barrier = 1;
			while (commit_barrier != 0);
		}
		mesg_count++;
	}
	
	//switch (mesg[mesg_len-1])
	switch (mesg[i+1])
	{
		case '1':
			//printf("commit to fs, typs: MKDIR\n");
			ret = mkdir(path, ps_info->batch_dir_mode);
			if (ret == -1)
			{
				ret = retry_commit(ps_info, path, 1);
				if (ret == -1)
				{
					printf("fail to commit to fs: typs: MKDIR, path: %s\n", path);
					return -1;
				}
			}
			break;

		case '2':
			//printf("commit to fs, typs: CREATE\n");	
			fd = creat(path, ps_info->batch_file_mode);
			if (fd == -1)
			{
				fd = retry_commit(ps_info, path, 2);
				if (fd == -1)
				{
					printf("fail to commit to fs: typs: CREATE, path: %s\n", path);
					return -1;
				}
			}
			char *val;
			char inline_data[INLINE_MAX];
			uint64_t cas, temp_cas;
			val = dmkv_get_cas(ps_info->kv_handle, path, &cas);
			temp_cas = cas;
			struct pacon_stat_server st;
			server_deseri_inline_data(&st, inline_data, val);
			// write inline data to the new file
			/*if (server_get_stat_flag(&st, STAT_inline) == 1 && st.size >0)
			{
				ret = pwrite(fd, inline_data, strlen(inline_data), 0);
				if (ret <= 0)
				{
					printf("write inline data error\n");
					return -1;
				}
			}*/
			close(fd);
			server_set_stat_flag(&st, STAT_file_created, 1);
			char value[PSTAT_SIZE+INLINE_MAX];
			server_seri_inline_data(&st, inline_data, value);
			ret = dmkv_cas(ps_info->kv_handle, path, value, st.size, temp_cas);
			while (ret != 0)
			{
				val = dmkv_get_cas(ps_info->kv_handle, path, &cas);
				temp_cas = cas;
				server_deseri_inline_data(&st, inline_data, val);
				server_set_stat_flag(&st, STAT_file_created, 1);
				server_seri_inline_data(&st, inline_data, value);
				ret = dmkv_cas(ps_info->kv_handle, path, value, st.size, temp_cas);
			}
			break;

		case '3':
			//printf("commit to fs, typs: RM\n");
			ret = remove(path);
			if (ret != 0)
			{
				ret = retry_commit(ps_info, path, 3);
				if (ret == -1)
				{
					printf("fail to commit to fs: typs: RM, path: %s\n", path);
					return -1;
				}
			}
			// del the invalid item in pacon if necessary
			ret = dmkv_del(ps_info->kv_handle, path);
			if (ret != 0)
			{
				printf("fail to rm invalid item from dmkv\n");
				return -1;
			}
			break;

		case '4':
			//printf("commit to fs, typs: RMDIR\n");
			reach_barrier = 2;
			while (reach_barrier != 0)
			break;

		case '5':
			//printf("commit to fs, typs: READDIR\n");
			reach_barrier = 2;
			while (reach_barrier != 0)
			break;

		case '6':
			//printf("commit to fs, typs: OWRITE\n");
			break;

		case '7':
			//printf("commit to fs, typs: FSYNC\n");
			break;

		case '0':
			//printf("commit to fs, typs: CHECKPOINT\n");
			reach_barrier = 2;
			while (reach_barrier != 0)
			break;

		default:
			printf("opt type error\n");
			return -1;
	}
	return 0;
}

// add the barrier in other node in the cluster, and set the commit_barrier in the local
int broadcast_barrier_begin(struct pacon_server_info *ps_info, uint32_t timestamp)
{
	int ret;
	// generate barrier mesg and broadcast it
	char c_mesg[TIMESTAMP_SIZE+3];
	char ts[TIMESTAMP_SIZE];
	sprintf(ts, "%d", timestamp);
	sprintf(c_mesg, "%s%s", BARRIER, ts);
	ret = server_broadcast(ps_info->s_comm, c_mesg);
	if (ret != 0)
		return -1;
	//commit_barrier = timestamp;
	if (reach_barrier != 2)
	{
		reach_barrier = 1;
	}
	return ret;
}

// del the barrier in other node in the cluster, and reset the commit_barrier in the local
int broadcast_barrier_end(struct pacon_server_info *ps_info)
{
	int ret;
	// generate barrier mesg and broadcast it
	char c_mesg[3];
	strcpy(c_mesg, DEL_BARRIER);
	ret = server_broadcast(ps_info->s_comm, c_mesg);
	if (ret != 0)
		return -1;
	commit_barrier = 0;
	reach_barrier = 0;
	mesg_count = 0;
	return ret;
}

void traversedir_dmkv_del(struct pacon_server_info *ps_info, char *path)
{
	int ret;
	char dir_new[PATH_MAX];
	DIR *pd;
	struct dirent *entry;
	pd = opendir(path);
	while ((entry = readdir(pd)) != NULL)
	{
		if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
			continue;
		int c_len = strlen(entry->d_name);
		int p_len = strlen(path);
		memcpy(dir_new, path, p_len);
		dir_new[p_len] = '/';
		memcpy(dir_new + p_len + 1, entry->d_name, c_len);
		dir_new[p_len+1+c_len] = '\0';
		dmkv_del(ps_info->kv_handle, dir_new);

		if (entry->d_type == DT_DIR)
		{
			traversedir_dmkv_del(ps_info, dir_new);
		} else {
			ret = remove(dir_new);
			if (ret != 0)
			{
				printf("traverse remove file error, %s\n", dir_new);
				return -1;
			}
		}
	}
	closedir(pd);
	ret = rmdir(path);
	if (ret != 0)
	{
		printf("traverse remove dir error, %s\n", path);
		return -1;
	}
}

int commit_to_fs_barrier(struct pacon_server_info *ps_info, char *mesg)
{
	int ret = -1;
	int fd; 
	int mesg_len = strlen(mesg);
	char path[PATH_MAX];
	//strncpy(path, mesg, mesg_len-2);
	//path[mesg_len-2] = '\0';
	int i;
	for (i = 0; i < mesg_len; ++i)
	{
		if (mesg[i] != ':')
			path[i] = mesg[i];
		else
			break;
	}
	path[i] = '\0';

	// get timestamp
	uint32_t timestamp;
	timestamp = atoi(mesg+i+2);
	ret = broadcast_barrier_begin(ps_info, timestamp);
	if (ret != 0)
	{
		printf("broadcast barrier error\n");
		return -1;
	}
	switch (mesg[i+1])
	{
		case '4':
			//printf("b commit to fs, typs: RMDIR\n");
			/*
			 * 1. call readdir to get all sub dirs and files under the target dir
			 * 2. del them in the dmkv
			 * 3. call rmdir
			 */
			while (reach_barrier != 2);
			traversedir_dmkv_del(ps_info, path);
			/*ret = rmdir(path);
			if (ret != 0)
			{
				printf("remove dir error, %s\n", path);
				return -1;
			}*/
			break;

		case '5':
			//printf("b commit to fs, typs: READDIR\n");
			break;

		case '0':
			//printf("b commit to fs, typs: CHECKPOINT\n");
			char cp_path[PATH_MAX];
			int i;
			for (i = strlen(path)-1; i >= 0; --i)
			{
				if (path[i] == '/')
					break;
			}
			sprintf(cp_path, "%s%s", CHECKPOINT_PATH, path+i);
			char rm_cmd[128];
			char rm_head = "rm -rf ";
			sprintf(rm_cmd, "%s%s", rm_head, cp_path);
			system(rm_cmd);

			char cmd[128];
			char head = "cp -rf ";
			sprintf(cmd, "%s%s%s%s", head, path, " ", CHECKPOINT_PATH);
			system(cmd);
			break;

		default:
			printf("barrier opt type error\n");
			return -1;
	}
	ret = broadcast_barrier_end(ps_info);

	return ret;
}

int handle_cluster_mesg(struct pacon_server_info *ps_info, char *mesg)
{
	int ret;
	uint32_t timestamp;
	switch (mesg[1])
	{
		case '8':
			//printf("set timestamp\n");
			timestamp = atoi(mesg+2);
			if (timestamp == 0)
			{
				printf("timestamp = 0, error\n");
				return -1;
			}
			commit_barrier = timestamp;
			while (remote_reach_barrier == 0)
			{
				if (time(NULL) - timestamp >= 1)
				{
					if (mesg_count == 0)
					{
						remote_reach_barrier = 1;
					} else {
						timestamp = time(NULL);
						mesg_count = 0;
					}
				}
			}
			break;

		case '9':
			//printf("del timestamp\n");
			commit_barrier = 0;
			remote_reach_barrier = 0;
			mesg_count = 0;
			break;	

		default:
			printf("handle cluster mesg error\n");
			return -1;
	}
	return 0;
}

void *listen_commit_mq(struct pacon_server_info *ps_info_t)
{
	printf("listening commit mq\n");
	struct pacon_server_info *ps_info = (struct pacon_server_info *)ps_info_t;
	int ret, ms_size;
	char mesg[PATH_MAX];
	while (1)
	{
		ms_size = zmq_recv(ps_info->subscriber, mesg, PATH_MAX, 0);
		if (ms_size == -1)
		{
			printf("commit mq get message error\n");
			continue;
		}
		mesg[ms_size] = '\0';
		ret = commit_to_fs(ps_info, mesg);
		if (ret != 0)
		{
			printf("commit opt to fs fail: %s\n", mesg);
			//return -1;
		}
	}
}

void *listen_local_rpc(struct pacon_server_info *ps_info_t)
{
	printf("listening local rpc\n");
	struct pacon_server_info *ps_info = (struct pacon_server_info *)ps_info_t;
	int ret, ms_size;
	char mesg[PATH_MAX];
	while (1)
	{
		ms_size = zmq_recv(ps_info->local_rpc_rep, mesg, PATH_MAX, 0);
		if (ms_size == -1)
		{
			printf("local rpc get message error\n");
			continue;
		}
		mesg[ms_size] = '\0';
		char rep[1];
		ret = commit_to_fs_barrier(ps_info, mesg);
		if (ret == 0)
		{
			rep[0] = '0';			
		} else {
			printf("some errors in local rpc\n");
			rep[0] = '1';
		}
		zmq_send(ps_info->local_rpc_rep, rep, 1, 0);
	}
}

void *listen_cluster_rpc(struct pacon_server_info *ps_info_t)
{
	printf("listening cluster rpc\n");
	struct pacon_server_info *ps_info = (struct pacon_server_info *)ps_info_t;
	int ret, ms_size;
	char mesg[PATH_MAX];
	while (1)
	{
		ms_size = zmq_recv(ps_info->cluster_rpc_rep, mesg, PATH_MAX, 0);
		if (ms_size == -1)
		{
			printf("cluster get message error\n");
			continue;
		}
		mesg[ms_size] = '\0';
		ret = handle_cluster_mesg(ps_info, mesg);
		char rep[1];
		if (ret == 0)
		{
			rep[0] = '0';			
		} else {
			printf("some errors in cluster rpc\n");
			rep[0] = '1';
		}
		zmq_send(ps_info->cluster_rpc_rep, rep, 1, 0);
	}
}



int main(int argc, char const *argv[])
{
	int ret;
	printf("Strat Pacon Server\n");
	struct pacon_server_info *ps_info = (struct pacon_server_info *)malloc(sizeof(struct pacon_server_info));
	ret = start_pacon_server(ps_info);
	if (ret != 0)
	{
		printf("strat pacon error\n");
		return -1;
	}

	pthread_t t_commit_mq;
	pthread_t t_local_rpc;
	pthread_t t_cluster_rpc;
	if (pthread_create(&t_commit_mq, NULL, listen_commit_mq, (void *)ps_info) == -1)
	{
		printf("create commit mq thread error\n");
		return -1;
	}
	if (pthread_create(&t_local_rpc, NULL, listen_local_rpc, (void *)ps_info) == -1)
	{
		printf("create local rpc thread error\n");
		return -1;
	}
	if (pthread_create(&t_cluster_rpc, NULL, listen_cluster_rpc, (void *)ps_info) == -1)
	{
		printf("create cluster rpc thread error\n");
		return -1;
	}

	/* old 
 	ret = listen_commit_mq(ps_info);
	if (ret != 0)
	{
		printf("listen mq error\n");
		return -1;
	}
	*/
	while(1);
	return 0;
}