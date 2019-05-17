/*
 *  written by Yubo
 */
#ifndef PCACHE_H
#define PCACHE_H

#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <fcntl.h>
#include <zmq.h>
#include "kv/dmkv.h"

/******* pacon configure *******/
#define PARENT_CHECK 1 // 0 is not check the parent dir when creating dir and file


#define KV_TYPE 0   // 0 is memc3, 1 is redis
#define MOUNT_PATH_MAX 128
#define PATH_MAX 128



struct pacon
{
	uint32_t node_num;
	//char node_list[NODE_MAX];
	uint32_t kv_type;
	char mount_path[MOUNT_PATH_MAX];
	// kv
	struct dmkv *kv_handle;
	// mq
	void *publisher;
	void *context;
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
	int open_flag;
	int hit; // 0 is miss in pacon, 1 is hit in pacon
	int fd; // fd only be used when the file data does not be cached in pacon
	int p_fd; // p_fd only be used when the file data are cached in pacon 
	char *buf; // buffer for inline file data, its largest size is 1KB
};


int set_root_path(struct pacon *pacon, char *r_path);

int init_pacon(struct pacon *pacon);

int free_pacon(struct pacon *pacon);

struct pacon_file * new_pacon_file(void);


/**************** fs interfaces ****************/

int pacon_open(struct pacon *pacon, const char *path, int flag, mode_t mode, struct pacon_file *p_file);

int pacon_close(struct pacon *pacon, struct pacon_file *p_file);

int pacon_create(struct pacon *pacon, const char *path, mode_t mode);

int pacon_mkdir(struct pacon *pacon, const char *path, mode_t mode);

int pacon_opendir(struct pacon *pacon, const char *path);

int pacon_readdir(struct pacon *pacon, const char *path, void *buf, off_t offset);

int pacon_getattr(struct pacon *pacon, const char *path, struct pacon_stat* st);

int pacon_rmdir(struct pacon *pacon, const char *path);

int pacon_rename(struct pacon *pacon, const char *path, const char *newpath);

int pacon_read(struct pacon *pacon, struct pacon_file *p_file, char *buf, size_t size, off_t offset);

int pacon_write(struct pacon *pacon, struct pacon_file *p_file, const char *buf, size_t size, off_t offset);

int pacon_fsync(struct pacon *pacon, int fd);

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


/**************** consistency area interfaces ****************/

int carea_joint(void);

int carea_split(void);


#endif