/*
 *  written by Yubo
 */
#include <stdio.h>
#include <malloc.h>
#include "pacon.h"
#include "kv/dmkv.h"

static struct dmkv *kv_handle;

struct commit_queue
{
	
};



int build_namespace(char *mount_path)
{
	return 0;
}

/* 
 * 1. inital kv
 * 2. build namespace & inital commit queue 
 */
int init_pacon(struct pacon *pacon)
{
	int ret;
	struct dmkv *kv = (struct dmkv *)malloc(sizeof(struct dmkv));
	ret = dmkv_init(kv);
	if (ret != 0)
	{
		printf("init dmkv fail\n");
		return -1;
	}
	kv_handle = kv;

	// build namespace
	ret = build_namespace(pacon->mount_path);

	return 0;
}

int free_pacon(struct pacon *pacon)
{
	dmkv_free(kv_handle);
	return 0;
} 

int pacon_open(const char *path)
{
	return 0;
}

int pacon_create(const char * path, mode_t mode)
{
	return fs_create(fs, path, mode);
}

int pacon_mkdir(const char *path, mode_t mode)
{
	int ret;
	cJSON *j_body;
	j_body = cJSON_CreateObject();
	cJSON_AddNumberToObject(j_body, "flags", 0);
	cJSON_AddNumberToObject(j_body, "mode", mode);
	cJSON_AddNumberToObject(j_body, "ctime", time(NULL));
	cJSON_AddNumberToObject(j_body, "atime", time(NULL));
	cJSON_AddNumberToObject(j_body, "mtime", time(NULL));
	cJSON_AddNumberToObject(j_body, "size", 0);
	cJSON_AddNumberToObject(j_body, "uid", getuid());
	cJSON_AddNumberToObject(j_body, "gid", getgid());
	cJSON_AddNumberToObject(j_body, "nlink", 0);
	//cJSON_AddNumberToObject(j_body, "opt", 0);
	//set_opt_flag(md, OP_mkdir, 1);
	char *value = cJSON_Print(j_body);
	ret = dmkv_set(kv_handle, path, value);
	return ret;
}

int pacon_opendir(const char *path)
{
	return fs_opendir(fs, path);
}

int pacon_readdir(const char *path, void *buf, off_t offset)
{
	// need implement a filler function
	return fs_readdir(fs, path, buf, filler, offset);
}

int pacon_getattr(const char* path, struct stat* st)
{
	return fs_getattr(fs, path, st);
}

int pacon_rmdir(const char *path)
{
	return fs_rmdir(fs, path);
}

int pacon_rename(const char *path, const char *newpath)
{
	return fs_rename(fs, path, newpath);
}

int pacon_read(const char *path, char *buf, size_t size, off_t offset)
{
	return fs_read(fs, path, buf, size, offset, file_info);
}

int pacon_write(const char *path, const char *buf, size_t size, off_t offset)
{
	return fs_write(fs, path, buf, size, offset, file_info);
}	

int pacon_release(const char *path)
{
	 return fs_release(fs, path, file_info);
}

int pacon_releasedir(const char *path)
{
	return fs_releasedir(fs, path, file_info);
}

int pacon_utimens(const char * path, const struct timespec tv[2])
{
	return fs_utimens(fs, path, tv);
}

int pacon_truncate(const char * path, off_t offset)
{
	return fs_truncate(fs, path, offset);
}

int pacon_unlink(const char * path)
{
	return fs_unlink(fs, path);
}

int pacon_chmod(const char * path, mode_t mode)
{
	return fs_chmod(fs, path, mode);
}

int pacon_chown(const char * path, uid_t owner, gid_t group)
{
	return fs_chown(fs, path, owner, group);
}

int pacon_access(const char * path, int amode)
{
	return fs_access(fs, path, amode);
}

int pacon_symlink(const char * oldpath, const char * newpath)
{
	return fs_symlink(fs, oldpath, newpath);
}

int pacon_readlink(const char * path, char * buf, size_t size)
{
	return fs_readlink(fs, path, buf, size);
}


