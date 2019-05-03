/*
 *  written by Yubo
 */
#ifndef PCACHE_H
#define PCACHE_H

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "kv/dmkv.h"

#define KV_TYPE 0   // 0 is memc3, 1 is redis
#define MOUNT_PATH_MAX 128



struct pacon
{
	uint32_t node_num;
	//char node_list[NODE_MAX];
	uint32_t kv_type;
	char mount_path[MOUNT_PATH_MAX];
};

struct pacon_file
{
	int hit; // 0 is miss in pacon, 1 is hit in pacon
	int fd; // fd only is used when the file data does not be cached in pacon
	char *buf;
};

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
	uint32_t fd;
};

int init_pacon(struct pacon *pacon);

int free_pacon(struct pacon *pacon);
 

/**************** fs interfaces ****************/

int pacon_open(const char *path, int flag, mode_t mode, struct pacon_file *p_file);

int pacon_close(struct pacon_file *p_file);

int pacon_create(const char *path, mode_t mode);

int pacon_mkdir(const char *path, mode_t mode);

int pacon_opendir(const char *path);

int pacon_readdir(const char *path, void *buf, off_t offset);

int pacon_getattr(const char *path, struct pacon_stat* st);

int pacon_rmdir(const char *path);

int pacon_rename(const char *path, const char *newpath);

int pacon_read(const char *path, struct pacon_file *p_file, char *buf, size_t size, off_t offset);

int pacon_write(const char *path, struct pacon_file *p_file, const char *buf, size_t size, off_t offset);

int pacon_fsync(int fd);

int pacon_release(const char *path);

int pacon_releasedir(const char *path);

int pacon_utimens(const char *path, const struct timespec tv[2]);

int pacon_truncate(const char *path, off_t offset);

int pacon_unlink(const char *path);

int pacon_batch_chmod(const char *path, mode_t mode);

int pacon_batch_chown(const char *path, uid_t owner, gid_t group);

int pacon_access(const char *path, int amode);

int pacon_symlink(const char *oldpath, const char * newpath);

int pacon_readlink(const char *path, char * buf, size_t size);


/**************** consistency area interfaces ****************/

int carea_joint(void);

int carea_split(void);


#endif