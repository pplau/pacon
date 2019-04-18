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
	if (ret != 0)
	{
		printf("build namespace error\n");
		return -1;
	}
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

int pacon_create(const char *path, mode_t mode)
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
	char *val;
	val = redisClusterCommand(pcache->redis,"GET %s", key);
	if (val == NULL)
		return -1;

	cJSON *j_body, *j_flags, *j_mode, *j_ctime, *j_atime, *j_mtime, *j_size, *j_uid, *j_gid, *j_nlink, *j_fd;
	j_body = cJSON_Parse(val);

	j_flags = cJSON_GetObjectItem(j_body, "flags");
	j_mode = cJSON_GetObjectItem(j_body, "mode");
	j_ctime = cJSON_GetObjectItem(j_body, "ctime");
	j_atime = cJSON_GetObjectItem(j_body, "atime");
	j_mtime = cJSON_GetObjectItem(j_body, "mtime");
	j_size = cJSON_GetObjectItem(j_body, "size");
	j_uid = cJSON_GetObjectItem(j_body, "uid");
	j_gid = cJSON_GetObjectItem(j_body, "gid");
	j_nlink = cJSON_GetObjectItem(j_body, "nlink");
	j_fd = cJSON_GetObjectItem(j_body, "fd");
	//j_opt = cJSON_GetObjectItem(j_body, "opt");

	st->flags = (uint32_t)j_flags->valueint;
	st->mode = (uint32_t)j_mode->valueint;
	st->ctime = (uint32_t)j_ctime->valueint;
	st->atime = (uint32_t)j_atime->valueint;
	st->mtime = (uint32_t)j_mtime->valueint;
	st->size = (uint32_t)j_size->valueint;
	st->uid = (uint32_t)j_uid->valueint;
	st->gid = (uint32_t)j_gid->valueint;
	st->nlink = (uint32_t)j_nlink->valueint;
	st->fd = (uint32_t)j_fd->valueint;
	//st->opt = (uint32_t)j_opt->valueint;
	return 0;
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


