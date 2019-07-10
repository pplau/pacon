/*
 *  written by Yubo
 */
#ifndef PACON_SERVER_H
#define PACON_SERVER_H

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/shm.h>

#include "../comm/comm.h"
#include "../kv/dmkv.h"

#define ASYNC_RPC 1  // 0 is sync rpc, 1 is async rpc
#define CHECKPOINT_TIME 0  // 0 mean no need checkpoint, >0 mean the perriod of checkpoint (second)
#define CHECKPOINT_PATH "/mnt/beegfs/pacon_checkpoint"

#define PATH_MAX 128
#define SP_LIST_MAX 16
#define RMDIRLIST_MAX 128
#define SHMKEY 123


struct carea_info
{

};

struct pacon_server_info
{
	int carea_num;
	struct carea_info *carea_list;
	char rec_mq_addr[28];
	int rec_mq_port;
	struct dmkv *kv_handle;

	// batch mode and owner
	int batch_dir_mode;
	int batch_file_mode;

	// commit mq
	void *subscriber;
	void *context;

	// local rpc
	void *local_rpc_rep;
	void *context_local_rpc;

	// cluster rpc
	void *cluster_rpc_rep;
	void *context_cluster_rpc;

	// cluster info
	struct servers_comm *s_comm;

	// for rmdir
	struct rmdir_record *rmdir_record;
};

#define PSTAT_SIZE 44 // 32 int * 11
#define INLINE_MAX 468 // PSTAT_SIZE + INLINE_MAX = 512B
struct pacon_stat_server
{
	uint32_t flags;   //  including the metadata type
	uint32_t mode;
	uint32_t ctime;
	uint32_t atime;
	uint32_t mtime;
	uint32_t size;
	uint32_t uid;
	uint32_t gid;
	uint32_t nlink;
	uint32_t open_counter;
	//uint32_t fd;
};


int start_pacon_server(struct pacon_server_info *ps_info);

int stop_pacon_server(struct pacon_server_info *ps_info);

int commit_to_fs(struct pacon_server_info *ps_info, char *mesg);

int new_carea(void);

int free_carea(void);

#endif