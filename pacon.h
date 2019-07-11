/*
 *  written by Yubo
 */
#ifndef PCACHE_H
#define PCACHE_H

#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <dirent.h>
#include <unistd.h> 
#include <fcntl.h>
#include <zmq.h>
#include <sys/shm.h>
#include "kv/dmkv.h"

/******* pacon configure *******/
#define PARENT_CHECK 1 // 0 is not check the parent dir when creating dir and file
#define ASYNC_RPC 1  // 0 is sync rpc, 1 is async rpc


#define KV_TYPE 0   // 0 is memc3, 1 is redis
#define MOUNT_PATH_MAX 128
#define PATH_MAX 128
#define SP_LIST_MAX 16
#define CR_JOINT_MAX 8
#define RMDIRLIST_MAX 128
#define SHMKEY 1

#define FSYNC_LOG_PATH "/mnt/beegfs/pacon_fsync_log/"
#define CHECKPOINT_PATH "/mnt/beegfs/pacon_checkpoint"
#define CRJ_INFO_PATH "./crj_info"



struct permission_info
{
	mode_t nom_dir_mode;
	mode_t nom_f_mode;
	int sp_num;
	char sp_path[SP_LIST_MAX][PATH_MAX];
	mode_t sp_dir_modes[SP_LIST_MAX];
	mode_t sp_f_modes[SP_LIST_MAX];
};

struct rmdir_record
{
	int rmdir_num;
	char rmdir_list[RMDIRLIST_MAX][PATH_MAX];
};

struct pacon
{
	uint32_t node_num;
	//char node_list[NODE_MAX];
	uint32_t kv_type;
	char mount_path[MOUNT_PATH_MAX];
	// kv
	struct dmkv *kv_handle;
	// commit mq
	void *publisher;
	void *context;
	// local rpc
	void *local_rpc_req;
	void *context_local_rpc;
	// fsync log file
	char fsync_logfile_path[PATH_MAX];
	int fsync_log_fd;
	int log_size;
	// batch permission info
	struct permission_info *perm_info;
	mode_t df_dir_mode;
	mode_t df_f_mode;
	// inter cregion control (used for cregion joint)
	int cr_num;
	char remote_cr_root[CR_JOINT_MAX][MOUNT_PATH_MAX];
	struct pacon *remote_pacon_list[CR_JOINT_MAX];
	// for async rmdir
	struct rmdir_record *rmdir_record;
};

#define PSTAT_SIZE 44 // 32 int * 11
#define INLINE_MAX 468 // PSTAT_SIZE + INLINE_MAX = 512B
struct pacon_stat
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

struct pacon_file
{
	// regular stat
	uint32_t flags;   //  including the metadata type
	uint32_t mode;
	uint32_t ctime;
	uint32_t atime;
	uint32_t mtime;
	uint32_t size;
	uint32_t uid;
	uint32_t gid;
	uint32_t nlink;
	//uint32_t fd;

	// file stat
	int open_flags;
	mode_t open_mode;
	int hit; // 0 is miss in pacon, 1 is hit in pacon
	int fd; // fd only be used when the file data does not be cached in pacon
	int p_fd; // p_fd only be used when the file data are cached in pacon 
	char *buf; // buffer for inline file data, its largest size is 1KB
	int hit_remote_cr;  // 1 is hit in other cregion
};


int set_root_path(struct pacon *pacon, char *r_path);

int init_pacon(struct pacon *pacon);

int free_pacon(struct pacon *pacon);

struct pacon_file * new_pacon_file(void);


/**************** fs interfaces ****************/

int pacon_open(struct pacon *pacon, const char *path, int flag, mode_t mode, struct pacon_file *p_file);

int pacon_close(struct pacon *pacon, struct pacon_file *p_file);

int pacon_create(struct pacon *pacon, const char *path, mode_t mode);

int pacon_create_write(struct pacon *pacon, const char *path, mode_t mode, const char *buf, size_t size, struct pacon_file *p_file);

int pacon_mkdir(struct pacon *pacon, const char *path, mode_t mode);

DIR * pacon_opendir(struct pacon *pacon, const char *path);

struct dirent * pacon_readdir(DIR *dir);

void pacon_closedir(struct pacon *pacon, DIR *dir);

int pacon_getattr(struct pacon *pacon, const char *path, struct pacon_stat* st);

int pacon_rename(struct pacon *pacon, const char *path, const char *newpath);

int pacon_read(struct pacon *pacon, char *path, struct pacon_file *p_file, char *buf, size_t size, off_t offset);

int pacon_write(struct pacon *pacon, char *path, struct pacon_file *p_file, const char *buf, size_t size, off_t offset);

int pacon_fsync(struct pacon *pacon, char *path, struct pacon_file *p_file);

int pacon_release(struct pacon *pacon, const char *path);

int pacon_releasedir(struct pacon *pacon, const char *path);

int pacon_utimens(struct pacon *pacon, const char *path, const struct timespec tv[2]);

int pacon_truncate(struct pacon *pacon, const char *path, off_t offset);

int pacon_unlink(struct pacon *pacon, const char *path);

int pacon_batch_chmod(struct pacon *pacon, const char *path, mode_t mode);

int pacon_batch_chown(struct pacon *pacon, const char *path, uid_t owner, gid_t group);

int pacon_access(struct pacon *pacon, const char *path, int amode);

int pacon_symlink(struct pacon *pacon, const char *oldpath, const char * newpath);

int pacon_readlink(struct pacon *pacon, const char *path, char * buf, size_t size);

void pacon_init_perm_info(struct permission_info *perm_info);

int pacon_set_permission(struct pacon *pacon, struct permission_info *perm_info);

/******* some notes in rmdir interface *********
 * these two funcs are cooperated to exectue rmdir 
 * app must clean the parent check table on each client after call rmdir
 * example:
 *		if (rank == 0)
 *			pacon_rmdir();
 *		else
 *			pacon_rmdir_clean();
 *		MPI_Barrier();  // it is optional
 *						// because each client holds an independent parent check table
 *						// but app needs to ensure that there is no ops under the deleted dir in the code behind this line
 ***********************************************/
int pacon_rmdir(struct pacon *pacon, const char *path);
int pacon_rmdir_clean(struct pacon *pacon);



/**************** consistent region interfaces ****************/

/***************************************************************
 * how to join two cregions:
 * 1. add the root path of the external cregion into ./crj_info
 * 2. add the node list of the external cregion into ./cr_joint_config
 * 3. call this func, the cr_num is the number of external cregions (not the total num)
 **************************************************************/
int cregion_joint(struct pacon *pacon, int cr_num);

int cregion_split(struct pacon *pacon, int cr_num);

int cregion_checkpoint(struct pacon *pacon);

int cregion_recover(struct pacon *pacon);


/**************** only for test ****************/

int evict_metadata(struct pacon *pacon);




#endif