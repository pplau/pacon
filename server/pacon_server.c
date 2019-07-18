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

#define TIMESTAMP_SIZE 11
#define INLINE_DATA_MAX 468

// opt type
#define CHECKPOINT ":0"
#define MKDIR ":1"
#define CREATE ":2"
#define RM ":3"
#define RMDIR ":4"
#define READDIR ":5"
#define OWRITE ":6"  // data size is larger than the INLINE_MAX, write it back to DFS
#define FSYNC ":7"
#define WRITE ":8"
#define BARRIER ":9"
#define RENAME ":A"
#define FLUSHDIR ":B"

// cluster mesg types
#define CL_RMDIRPRE ":1"
#define CL_RMDIRPOST ":2"
#define BARRIER_ASYNC ":6"
#define BARRIER_ASYNC_WAIT ":7"
#define BARRIER ":8"
#define DEL_BARRIER ":9"


static int reach_barrier_count = 0;
static uint32_t commit_barrier = 0;  // 0 is not barrier, != 0 is timestamp
static int reach_barrier = 0;        // 0 is not barrier
									 // 1 is local rpc receives a barrier but commit mq doesn't reach barrier
									 // 2 is commit mq already reached to the barrier
static int remote_reach_barrier = 0;
static int mesg_count = 0; 

static int sp_permission = -1;  // 0 means using default setting, 1 means using special setting
static struct permission_info perm_info; 

static char local_ip[17];

// new version
static int client_num = 0;
static int current_barrier_id = 0;
static int waiting_barrier_id = 0;
static int pre_barrier = 0;
static int barrier[BARRIER_ID_MAX] = {0};
static struct barrier_info barrier_info;
static int barrier_id_lock = 0;


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
	char temp[24];  // old 16
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

void get_local_addr_cnum(void)
{
	FILE *fp;
	fp = fopen("../local_config", "r");
	if (fp == NULL)
	{
		printf("cannot open config file\n");
		return -1;
	}
	char temp[24];
	fgets(temp, 24, fp);
	int i;
	for (i = 0; i < 24; ++i)
	{
		if ((temp[i] >= '0' && temp[i] <= '9') || temp[i] == '.')
		{
			local_ip[i] = temp[i];
		} 
		if (temp[i] == ':')
		{
			local_ip[i] = '\0';
			break;
		}
	}
	client_num = atoi(temp+i+1);
}

/* 
 * ret = 1 means it is sub file/dir, 0 means it is not sub file/dir, 2 mean they are the same path
 */
int child_cmp(char *path, char *p_path, int recursive)
{
	int len = strlen(path);
	int p_len = strlen(p_path); 
	if (len < p_len)
		return 0;    // not the child dentry

	int i;
	for (i = 0; i < p_len; ++i)
	{
		if (path[i] == p_path[i])
			continue;
		if (path[i] != p_path[i])
			return 0;
	}
	if (path[i] != '/' && p_len != len)
		return 0;
	if (p_len != len)
		return 1;
	if (p_len == len)
		return 2;
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

	// init kv for barrier commit thread
	struct dmkv *kv_b = (struct dmkv *)malloc(sizeof(struct dmkv));
	set_dmkv_config_type(1);
	ret = dmkv_init(kv_b);
	if (ret != 0)
	{
		printf("init dmkv for barrier commit thread fail\n");
		return -1;
	}
	ps_info->kv_handle_for_barrier = kv_b;

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

	// init share memory
	if (ASYNC_RPC == 1)
	{
    	int shmid;
    	void* shm;
    	shmid = shmget((key_t)SHMKEY, sizeof(struct rmdir_record), 0666 | IPC_CREAT);
    	if (shmid == -1)
    	{
    		printf("shmget error\n");
    		return -1;
    	}
    	shm = shmat(shmid, (void*)0, 0);
    	if (shm == (void*) -1)
    	{
    		printf("shmat error\n");
    		return -1;
    	}
    	ps_info->rmdir_record = (struct rmdir_record *)shm;
    	ps_info->rmdir_record->rmdir_num = 0;
	}

	// init barrier info
	char opt_key[PATH_MAX];
	int i;
	for (i = 0; i < BARRIER_ID_MAX; ++i)
	{
		sprintf(opt_key, "%s%c%d", BARRIER_OPT_COUNT_KEY, '.', i);
		ret = dmkv_set(ps_info->kv_handle_for_barrier, opt_key, "0", strlen("0"));
		if (ret != 0)
		{
			//printf("init barrier error: fail to set BARRIER_OPT_COUNT_KEY in kv\n");
			//return -1;
		}
	}
	get_local_addr_cnum();
	printf("init barrier info: local ip %s, client num %d\n", local_ip, client_num);
	char b_key[PATH_MAX];
	for (i = 0; i < BARRIER_ID_MAX; ++i)
	{
		sprintf(b_key, "%s%c%d", local_ip, '.', i);
		ret = dmkv_set(ps_info->kv_handle_for_barrier, b_key, "0", strlen("0"));
		if (ret != 0)
		{
			//printf("init barrier error: fail to set LOCAL_IP in kv\n");
			//return -1;
		}
	}
	for (i = 0; i < BARRIER_ID_MAX; ++i)
	{
		barrier_info.barrier[i] = 0;
	}
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

	if (ASYNC_RPC == 1)
	{
		ret = shmdt(ps_info->rmdir_record);
		if (ret == -1)
		{
			printf("free share memory error\n");
			return -1;
		}
	}
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

int fsync_from_log(struct pacon_server *ps_info, char *path)
{
	int ret;
	char f_path[PATH_MAX];
	char log_path[PATH_MAX];
	int i;
	int loc = 0;
	int flag = 0;
	int offset = 0;
	int size = 0;
	for (i = 0; i < strlen(path); ++i)
	{
		if (path[i] == '|')
		{
			loc = i;
			if (flag == 0)
				f_path[loc] = '\0';
			if (flag == 1)
				log_path[loc] = '\0';
			flag++;
			continue;
		}
		if (flag == 0)
		{
			f_path[i] = path[i];
		} else if (flag == 1) {
			log_path[i-loc-1] = path[i];
		} else if (flag == 2) {
			if (path[i-1] == '|')
				offset = atoi(path+i);
			else
				continue;
		} else if (flag == 3) {
			if (path[i-1] == '|')
				size = atoi(path+i);
			else
				continue;
		} else {
			printf("fsync from log: reslove mesg error\n");
			return -1;
		}
	}
	int log_fp;
	log_fp = open(log_path, O_RDONLY);
	char data[INLINE_DATA_MAX];
	ret = pread(log_fp, data, size, offset);
	if (ret == -1)
	{
		printf("read fsync log error\n");
		return -1;
	}
	close(log_fp);

	int fp;
	while ((fp = open(f_path, O_RDWR)) < 0);
	ret = pwrite(fp, data, size, 0);
	if (ret == -1)
	{
		printf("write file error\n");
		return -1;
	}
	ret = fsync(fp);
	if (ret != 0)
	{
		printf("fsync file error\n");
		return -1;
	}
	close(fp);
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
	while (reach_barrier != 2);
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

int broadcast_barrier_begin_new(struct pacon_server_info *ps_info, uint32_t timestamp, int opt_barrier_id)
{
	int ret;
	int barrier_opt_count;
	char *val;
	char new_val[PATH_MAX];
	uint64_t cas;

 	// wait for all nodes reach barrier
 	while (barrier[opt_barrier_id] == 0);
 	int i, reach;
 	for (i = 0; i < ps_info->kv_handle_for_barrier->c_info->node_num; ++i)
	{
		if (strcmp(local_ip, ps_info->kv_handle_for_barrier->c_info->node_list[i]) == 0)
			continue;
		char b_key[PATH_MAX];
		sprintf(b_key, "%s%c%d", ps_info->kv_handle_for_barrier->c_info->node_list[i], '.', opt_barrier_id);
		reach = 0;
		while (reach == 0)
		{
			val = dmkv_get(ps_info->kv_handle_for_barrier, b_key);
			if (val != NULL)
			{
				reach = atoi(val);
			}
			usleep(rand()%50000);
		}
	}
	return 0;
}

int broadcast_barrier_end_new(struct pacon_server_info *ps_info, int opt_barrier_id)
{
	int ret;
	int barrier_opt_count;
	char *val;
	char new_val[PATH_MAX];
	uint64_t cas;

	// reduce barrier opt count
	char opt_key[PATH_MAX];
	sprintf(opt_key, "%s%c%d", BARRIER_OPT_COUNT_KEY, '.', opt_barrier_id);
getoptc:
	val = dmkv_get_cas(ps_info->kv_handle_for_barrier, opt_key, &cas);
	if (val == NULL)
	{
		//printf("get barrier opt count error\n");
		//return -1;
		goto getoptc;
	}
	barrier_opt_count = atoi(val);
	barrier_opt_count--;
	sprintf(new_val, "%d", barrier_opt_count);
 	ret = dmkv_cas(ps_info->kv_handle_for_barrier, opt_key, new_val, strlen(new_val), cas);
 	if (ret != 0)
 	{
 		usleep(rand()%50000);
 		goto getoptc;
 	}

 	// no concurrent opt, remove the barrier in the local and broadcast del_barrier mesg
 	if (barrier_opt_count == 0)
 	{	
		barrier[opt_barrier_id] = 0;
		char b_key[PATH_MAX];
		sprintf(b_key, "%s%c%d", local_ip, '.', opt_barrier_id);
reset:
		ret = dmkv_set(ps_info->kv_handle_for_barrier, b_key, "0", strlen("0"));
		if (ret != 0)
		{
			usleep(rand()%50000);
			goto reset;
		}
	 	char c_mesg[3];
		strcpy(c_mesg, DEL_BARRIER);
		ret = server_broadcast(ps_info->s_comm, c_mesg);
		waiting_barrier_id++;
 	}
 	return 0;
}

// 
int broadcast_barrier_begin_async(struct pacon_server_info *ps_info, uint32_t timestamp)
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
	return ret;
}

// 
int broadcast_barrier_end_async(struct pacon_server_info *ps_info)
{
	int ret;
	// generate barrier mesg and broadcast it
	char c_mesg[3];
	strcpy(c_mesg, DEL_BARRIER);
	ret = server_broadcast(ps_info->s_comm, c_mesg);
	if (ret != 0)
		return -1;
	return ret;
}

// 
int broadcast_wait_barrier(struct pacon_server_info *ps_info)
{
	int ret;
	// generate barrier mesg and broadcast it
	char c_mesg[3];
	strcpy(c_mesg, BARRIER_ASYNC_WAIT);
	ret = server_broadcast(ps_info->s_comm, c_mesg);
	if (ret != 0)
		return -1;
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
		dmkv_del(ps_info->kv_handle_for_barrier, dir_new);

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

void traversedir_dmkv_flush(struct pacon_server_info *ps_info, char *path)
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
		dmkv_del(ps_info->kv_handle_for_barrier, dir_new);

		if (entry->d_type == DT_DIR)
		{
			traversedir_dmkv_flush(ps_info, dir_new);
		}
	}
	closedir(pd);
}

void checkpoint(char *path)
{
	char cp_path[128];
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
}

void traversedir_dmkv_rename(struct pacon_server_info *ps_info, char *oldpath, char *newpath)
{
	int ret;
	char dir_new[PATH_MAX];
	DIR *pd;
	struct dirent *entry;
	pd = opendir(oldpath);
	while ((entry = readdir(pd)) != NULL)
	{
		if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
			continue;
		int c_len = strlen(entry->d_name);
		int p_len = strlen(oldpath);
		memcpy(dir_new, oldpath, p_len);
		dir_new[p_len] = '/';
		memcpy(dir_new + p_len + 1, entry->d_name, c_len);
		dir_new[p_len+1+c_len] = '\0';

		if (entry->d_type == DT_DIR)
		{
			ret = dmkv_del(ps_info->kv_handle_for_barrier, dir_new);
			if (ret != 0)
			{
				printf("traverse rename: del dir error%s\n", dir_new);
				return;
			}
			traversedir_dmkv_rename(ps_info, dir_new, newpath);
		} else {
			ret = dmkv_del(ps_info->kv_handle_for_barrier, dir_new);
			if (ret != 0)
			{
				printf("traverse rename del file error%s\n", dir_new);
				return;
			}
		}
	}
	closedir(pd);
}

void rename_update_dc(struct pacon_server_info *ps_info, char *path)
{
	int ret;
	char oldpath[PATH_MAX/2];
	char newpath[PATH_MAX/2];
	if (strlen(path) <= 1)
	{
		printf("rename error in pacon server: path too short\n");
		return;
	}
	int i;
	int loc = 0;
	for (i = 0; i < PATH_MAX; ++i)
	{
		if (path[i] == '\0')
		{
			newpath[i-loc-1] = '\0';
			break;
		}
		if (loc == 0 && path[i] != '-')
		{
			oldpath[i] = path[i];
		} else if (path[i] == '-') {
			loc = i;
			oldpath[i] = '\0';
		} else if (loc > 0) {
			newpath[i-loc-1] = path[i];
		}
	}

	// update chlidren in the distributed cache of the old path
	traversedir_dmkv_rename(ps_info, oldpath, newpath);
	ret = rename(oldpath, newpath);
	if (ret != 0)
	{
		printf("rename error in pacon server: %s\n", oldpath);
		return;
	}
}

// remote = 0 means taht it is used by local, =1 means that it is used by cluster handler
int rmdir_pre(struct pacon_server_info *ps_info, char *path, int remote)
{
	int ret, i;
	struct rmdir_record *rmdir_record;
	int loc = ps_info->rmdir_record->rmdir_num;
	int shmkey = ps_info->rmdir_record->shmid_count;
	if (loc + 1 > shmkey * RMDIRLIST_MAX)
	{
    	int shmid;
    	void* shm;
    	shmkey++;
    	shmid = shmget((key_t)shmkey, sizeof(struct rmdir_record), 0666 | IPC_CREAT);
    	if (shmid == -1)
    	{
    		printf("shmget error\n");
    		return -1;
    	}
    	shm = shmat(shmid, (void*)0, 0);
    	if (shm == (void*) -1)
    	{
    		printf("shmat error\n");
    		return -1;
    	}
    	rmdir_record = (struct rmdir_record *)shm;
    	loc = loc % RMDIRLIST_MAX;
	} else {
		rmdir_record = ps_info->rmdir_record;
	}

	for (i = 0; i < strlen(path); ++i)
	{
		rmdir_record->rmdir_list[loc][i] = path[i];
	}
	if (i >= PATH_MAX)
	{
		printf("path too long\n");
		return -1;
	}
	rmdir_record->rmdir_list[loc][i] = '\0';

	// the first rmdir_record stores the total rmdir num
	ps_info->rmdir_record->rmdir_num++;
	if (ps_info->rmdir_record->rmdir_num <= (shmkey-1)*RMDIRLIST_MAX &&
		shmkey > 1)
		ps_info->rmdir_record->shmid_count++;

	if (remote == 0)
	{
		char c_mesg[PATH_MAX+3];
		sprintf(c_mesg, "%s%s", CL_RMDIRPRE, path);
		ret = server_broadcast(ps_info->s_comm, c_mesg);
		if (ret != 0)
			return -1;
	}
	return 0;
}

// remote = 0 means taht it is used by local, =1 means that it is used by cluster handler
int rmdir_post(struct pacon_server_info *ps_info, char *path, int remote)
{
	traversedir_dmkv_del(ps_info, path);
	int ret, i;
	struct rmdir_record *rmdir_record, *last_r;
	int last_shmid;
	void *last_shm;
	int last_shmkey = ps_info->rmdir_record->shmid_count;
	if (last_shmkey > 1)
	{
		last_shmid = shmget((key_t)last_shmkey, sizeof(struct rmdir_record), 0666 | IPC_CREAT);
		if (last_shmid == -1)
		{
			printf("shmget error\n");
			return -1;
		}
		last_shm = shmat(last_shmid, (void*)0, 0);
		if (last_shm == (void*) -1)
		{
			printf("shmat error\n");
			return -1;
		}
		last_r = (struct rmdir_record *)last_shm;
	} else {
		last_r = ps_info->rmdir_record;
	}

	for (i = 1; i <= ps_info->rmdir_record->shmid_count; ++i)
	{
		if (i = 1)
		{
			rmdir_record = ps_info->rmdir_record;
		} else {
	    	int shmid;
	    	void* shm;
	    	shmid = shmget((key_t)i, sizeof(struct rmdir_record), 0666 | IPC_CREAT);
	    	if (shmid == -1)
	    	{
	    		printf("shmget error\n");
	    		return -1;
	    	}
	    	shm = shmat(shmid, (void*)0, 0);
	    	if (shm == (void*) -1)
	    	{
	    		printf("shmat error\n");
	    		return -1;
	    	}
	    	rmdir_record = (struct rmdir_record *)shm;
		}

		int j, pos, last_pos;
		for (j = 0; j < ps_info->rmdir_record->rmdir_num; ++j)
		{
			pos = j - (RMDIRLIST_MAX * (i-1));
			last_pos = (ps_info->rmdir_record->rmdir_num) % RMDIRLIST_MAX;
			if (child_cmp(path, rmdir_record->rmdir_list[pos], 1) == 2)
			{
				// move the last rmdir to this slot
				if (i == last_shmkey && pos == last_pos)
				{
					sprintf(last_r->rmdir_list[last_pos], "%s", "\0");
				} else {
					sprintf(rmdir_record->rmdir_list[pos], "%s", last_r->rmdir_list[last_pos]);
					sprintf(last_r->rmdir_list[last_pos], "%s", "\0");
				}
				ps_info->rmdir_record->rmdir_num--;
				if (ps_info->rmdir_record->rmdir_num <= (last_shmkey-1)*RMDIRLIST_MAX &&
					last_shmkey > 1)
					ps_info->rmdir_record->shmid_count--;
			}
		}

		if (i != 1)
		{
			ret = shmdt(rmdir_record);
			if (ret == -1)
			{
				printf("free share memory error\n");
				return -1;
			}
		}
	}

	if (remote == 0)
	{
		char c_mesg[PATH_MAX+3];
		sprintf(c_mesg, "%s%s", CL_RMDIRPOST, path);
		ret = server_broadcast(ps_info->s_comm, c_mesg);
		if (ret != 0)
			return -1;
	}
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

	// batch permission
	if (sp_permission == -1)
	{
		char *nom_dir_mode = "nom_dir_mode";
		char *nom_f_mode = "nom_f_mode";
		char *sp_path = "sp_path";
		char *nom_dir_mode_val;
		char *nom_f_mode_val;
		char *sp_val;
		nom_dir_mode_val = dmkv_get(ps_info->kv_handle, nom_dir_mode);
		if (nom_dir_mode_val == NULL)
		{
			sp_permission = 0;
		} else {
			perm_info.nom_dir_mode = atoi(nom_dir_mode_val);
			nom_f_mode_val = dmkv_get(ps_info->kv_handle, nom_dir_mode);
			perm_info.nom_f_mode = atoi(nom_f_mode_val);
			sp_val = dmkv_get(ps_info->kv_handle, sp_path);
			int j = 0;
			int pos = j;
			int val_len = strlen(sp_val);
			perm_info.sp_num = 0;
			for (; j < strlen(sp_val); ++j)
			{
				if (sp_val[j] != ':' && sp_val[j] != '|')
				{
					perm_info.sp_path[perm_info.sp_num][pos] = sp_val[j];
				} else if (sp_val[j] == ':') {
					char mode_tmp[5];
					j++;
					int pos_tmp = j;
					while (sp_val[j] != 'd' && sp_val[j] != 'f')
					{
						mode_tmp[j-pos_tmp] = sp_val[j];
						j++;
					}
					mode_tmp[j-pos_tmp] = '\0';
					if (sp_val[j] == 'd')
					{
						perm_info.sp_dir_modes[perm_info.sp_num] = atoi(mode_tmp);
						perm_info.sp_f_modes[perm_info.sp_num] = 999;
					} else {
						perm_info.sp_f_modes[perm_info.sp_num] = atoi(mode_tmp);
						perm_info.sp_dir_modes[perm_info.sp_num] = 999;
					}
				} else if (sp_val[j] == '|') {
					pos = 0;
					perm_info.sp_num++;
				}
			}
			perm_info.sp_num++;
			sp_permission = 1;
		}
	}

	/* only the remote barrier will go into this logic
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
	}*/

	while(barrier[waiting_barrier_id] == 1);
	
	switch (mesg[i+1])
	{
		case '1':
			//printf("commit to fs, typs: MKDIR\n");
			if (sp_permission == 0)
			{
				ret = mkdir(path, S_IFDIR | 0755);
			} else if (sp_permission == 1) {
				ret = mkdir(path, S_IFDIR | 0755);
			} else {
				printf("batch permission error\n");
				return -1;
			} 

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
			if (sp_permission == 0)
			{
				fd = creat(path, S_IFREG | 0644);
			} else if (sp_permission == 1) {
				fd = creat(path, S_IFREG | 0644);
			} else {
				printf("batch permission error\n");
				return -1;
			}

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
			if (ASYNC_RPC == 0)
			{
				reach_barrier = 2;
				while (reach_barrier != 0);
			} else {
				ret = broadcast_wait_barrier(ps_info);
				if (ret != 0)
				{
					printf("wait remote barrier error\n");
					return -1;
				}
				ret = commit_to_fs_barrier_post(ps_info, mesg);
				if (ret != 0)
				{
					printf("barrier commit post error\n");
					return -1;
				}
			}
			break;

		case '5':
			//printf("commit to fs, typs: READDIR\n");
			reach_barrier = 2;
			while (reach_barrier != 0);
			break;

		case '6':
			//printf("commit to fs, typs: OWRITE\n");
			break;

		case '7':
			//printf("commit to fs, typs: FSYNC\n");
			ret = fsync_from_log(ps_info, path);
			if (ret != 0)
			{
				printf("pacon server: fsync from log error%s\n", path);
				return -1;
			}
			break;

		case '8':
			//printf("commit to fs, typs: WRITE\n");
			fd = open(path, O_RDWR);
			if (fd == -1)
				break;
			char *val1;
			char inline_data1[INLINE_MAX];
			struct pacon_stat_server st1;
			val1 = dmkv_get(ps_info->kv_handle, path);
			server_deseri_inline_data(&st1, inline_data1, val1);
			ret = pwrite(fd, inline_data1, st1->size, 0);
			if (ret <= 0)
			{
				printf("write inline data error\n");
				close(fd);
				return -1;
			}
			close(fd);
			break;

		case '9':
			//printf("commit to fs, typs: BARRIER\n");
			/*if (pre_barrier == 0)
			{
				if (atoi(path) == current_barrier_id + 1)
				{
					current_barrier_id++;
				} else {
					printf("current_barrier_id error\n");
				}
				pre_barrier = 1;
			}
			if (atoi(path) > current_barrier_id)
			{
				printf("handle the barrier after current barrier id\n");
			}*/

			//barrier_info.barrier[current_barrier_id]++;
			if (path[0] != '0')
			{
				char opt_num_s[PATH_MAX];
				int opt_num;
				for (i = 0; i < strlen(path); ++i)
				{
					if (path[i] == '|')
						break;
					opt_num_s[i] = path[i];
				}
				opt_num_s[i] = '\0';
				opt_num = atoi(opt_num_s);
				// increase barrier opt count
				int barrier_opt_count;
				char *val;
				char new_val[PATH_MAX];
				uint64_t cas;
				char opt_key[PATH_MAX];
				sprintf(opt_key, "%s%c%d", BARRIER_OPT_COUNT_KEY, '.', atoi(path+2)-1);
			getoptc:
				val = dmkv_get_cas(ps_info->kv_handle_for_barrier, opt_key, &cas);
				if (val == NULL)
				{
					//printf("get barrier opt count error\n");
					//return -1;
					goto getoptc;
				}
				barrier_opt_count = atoi(val);
				barrier_opt_count = barrier_opt_count + opt_num;
				sprintf(new_val, "%d", barrier_opt_count);
			 	ret = dmkv_cas(ps_info->kv_handle_for_barrier, opt_key, new_val, strlen(new_val), cas);
			 	if (ret != 0)
			 	{
			 		usleep(rand()%50000);
			 		goto getoptc;
			 	}
			}
			barrier_info.barrier[atoi(path+2)-1]++;
			if (barrier_info.barrier[atoi(path+2)-1] == client_num)
			{
				barrier[atoi(path+2)-1] = 1;
				char b_key[PATH_MAX];
				sprintf(b_key, "%s%c%d", local_ip, '.', atoi(path+2)-1);
				ret = dmkv_set(ps_info->kv_handle, b_key, "1", strlen("1"));
				if (ret != 0)
				{
					printf("set barrier in kv error\n");
					return -1;
				}
				//pre_barrier = 0;
				//current_barrier_id++;
				//sprintf(b_key, "%s%c%d", local_ip, '.', current_barrier_id);
				//ret = dmkv_set(ps_info->kv_handle, b_key, "0", strlen("0"));
			}
			break;

		case '0':
			//printf("commit to fs, typs: CHECKPOINT\n");
			reach_barrier = 2;
			while (reach_barrier != 0);
			break;

		case 'A':
			//printf("commit to fs, typs: RENAME\n");
			reach_barrier = 2;
			while (reach_barrier != 0);
			return 0;

		case 'B':
			//printf("commit to fs, typs: FLUSHDIR\n");
			reach_barrier = 2;
			while (reach_barrier != 0);
			return 0;

		default:
			printf("opt type error, type: %c\n", mesg[i+1]);
			//return -1;
	}
	return 0;
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
	char b_id[PATH_MAX];
	int barrier_id;
	for (i = 0; mesg[i] != '/'; ++i)
	{
		b_id[i] = mesg[i];
	}
	if (i != 0)
	{
		b_id[i] = '\0';
		barrier_id = atoi(b_id) - 1;
	}
	int j;
	for (j = 0; i < mesg_len; ++i)
	{
		if (mesg[i] != ':')
			path[j] = mesg[i];
		else
			break;
		j++;
	}
	path[j] = '\0';

	// get timestamp
	uint32_t timestamp;
	timestamp = atoi(mesg+i+2);
	//ret = broadcast_barrier_begin(ps_info, timestamp);
	//while (barrier_id_lock == 1);
	//barrier_id_lock = 1;
	ret = broadcast_barrier_begin_new(ps_info, timestamp, barrier_id);
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
			traversedir_dmkv_del(ps_info, path);
			if (ASYNC_RPC == 1)
				rmdir_post(ps_info, path, 0);
			break;

		case '5':
			//printf("b commit to fs, typs: READDIR\n");
			break;

		case '0':
			//printf("b commit to fs, typs: CHECKPOINT\n");
			//while (reach_barrier != 2);
			checkpoint(path);
			break;

		case 'A':
			//printf("b commit to fs, typs: RENAME\n");
			//while (reach_barrier != 2);
			rename_update_dc(ps_info, path);
			break;

		case 'B':
			//printf("b commit to fs, typs: FLUSHDIR\n");
			//while (reach_barrier != 2);
			traversedir_dmkv_flush(ps_info, path);
			break;

		default:
			printf("barrier opt type error\n");
			return -1;
	}
	//ret = broadcast_barrier_end(ps_info);
	ret = broadcast_barrier_end_new(ps_info, barrier_id);
	//barrier_id_lock = 0;

	return ret;
}

int commit_to_fs_barrier_pre(struct pacon_server_info *ps_info, char *mesg)
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

	// test version
	uint32_t timestamp;
	timestamp = atoi(mesg+i+2);
	ret = broadcast_barrier_begin_async(ps_info, timestamp);
	if (ret != 0)
	{
		printf("broadcast barrier error\n");
		return -1;
	}
	// test version

	switch (mesg[i+1])
	{
		case '4':
			//printf("b commit to fs, typs: RMDIR\n");
			ret = rmdir_pre(ps_info, path, 0);
			if (ret != 0)
			{
				printf("rmdir pre error\n");
				return -1;
			}
			break;

		default:
			printf("barrier pre opt type error\n");
			return -1;
	}
	return 0;
}

int commit_to_fs_barrier_post(struct pacon_server_info *ps_info, char *mesg)
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

	switch (mesg[i+1])
	{
		case '4':
			//printf("b commit to fs, typs: RMDIR\n");
			ret = rmdir_post(ps_info, path, 0);
			if (ret != 0)
			{
				printf("rmdir post error\n");
				return -1;
			}
			break;

		default:
			printf("barrier pre opt type error\n");
			return -1;
	}
	ret = broadcast_barrier_end_async(ps_info);
	return 0;
}

int handle_cluster_mesg(struct pacon_server_info *ps_info, char *mesg)
{
	int ret;
	uint32_t timestamp;
	switch (mesg[1])
	{
		case '1':
			// printf("rmdir pre\n");
			ret = rmdir_pre(ps_info, mesg+2, 1);
			if (ret != 0)
			{
				printf("rmdir pre error\n");
				return -1;
			}
			break;

		case '2':
			// printf("rmdir pre\n");
			ret = rmdir_post(ps_info, mesg+2, 1);
			if (ret != 0)
			{
				printf("rmdir post error\n");
				return -1;
			}
			break;

		case '6':
			// printf("async barrier\n");
			timestamp = atoi(mesg+2);
			if (timestamp == 0)
			{
				printf("timestamp = 0, error\n");
				return -1;
			}
			commit_barrier = timestamp;
			break;

		case '7':
			// printf("async wait barrier\n");
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
			//printf("del barrier\n");
			/*commit_barrier = 0;
			remote_reach_barrier = 0;
			mesg_count = 0;*/
			//barrier_info.barrier[waiting_barrier_id] = 0;
			//while (barrier_id_lock = 1)
			//barrier_id_lock = 1;
			barrier[waiting_barrier_id] = 0;
			waiting_barrier_id++;
			//barrier_id_lock = 0;
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

void *listen_local_rpc_async(struct pacon_server_info *ps_info_t)
{
	printf("listening local rpc\n");
	struct pacon_server_info *ps_info = (struct pacon_server_info *)ps_info_t;
	int ret, ms_size, i;
	char mesg[PATH_MAX];
	while (1)
	{
		i = 0;
		char rep[1];
		ms_size = zmq_recv(ps_info->local_rpc_rep, mesg, PATH_MAX, 0);
		if (ms_size == -1)
		{
			printf("local rpc get message error\n");
			continue;
		}
		mesg[ms_size] = '\0';

		while (mesg[i] != ':')
			i++;

		if (mesg[i+1] == '4')
		{
			/* async method */
			ret = commit_to_fs_barrier_pre(ps_info, mesg);
			if (ret != 0)
			{
				printf("async pre error%s\n", mesg);
				rep[0] = '1';
			} else {
				rep[0] = '0';
			}
			zmq_send(ps_info->local_rpc_rep, rep, 1, 0);
			/*ret = commit_to_fs_barrier(ps_info, mesg);
			if (ret != 0)
				printf("some errors in local rpc\n");*/

		} else {
			/* sync method */
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

	if (ASYNC_RPC == 0)
	{
		if (pthread_create(&t_local_rpc, NULL, listen_local_rpc, (void *)ps_info) == -1)
		{
			printf("create local rpc thread error\n");
			return -1;
		}
	} else {
		if (pthread_create(&t_local_rpc, NULL, listen_local_rpc_async, (void *)ps_info) == -1)
		{
			printf("create local rpc thread error\n");
			return -1;
		}	
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
