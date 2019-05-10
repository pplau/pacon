/*
 *  written by Yubo
 */
#include <stdio.h>
#include <malloc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "pacon.h"
#include "kv/dmkv.h"
#include "./lib/cJSON.h"

#define BUFFER_SIZE 64

// opt type
#define MKDIR ":1"
#define CREATE ":2"
#define RM ":3"
#define RMDIR ":4"
#define LINK ":5"

static struct dmkv *kv_handle;
static char mount_path[MOUNT_PATH_MAX];



/* 
 * ret = 0 means recursive sub file/dir, 1 means not recursive sub file/dir,  
 * recursive = 0 means not recursive, =1 means recursive search all sub dir 
 */
int child_cmp(char *path, char *p_path, int recursive)
{
	int len = strlen(path);
	int p_len = strlen(p_path); 
	if (len <= p_len)
		return 0;    // not the child dentry

	int i;
	if (recursive == 0)
	{
		for (i = 0; i < p_len; ++i)
		{
			if (path[i] == p_path[i])
				continue;
			if (path[i] != p_path[i])
				return 0;
		}
		if (p_len > 1 && path[i] != '/')    // just name prefix is the same and not the root
			return 0;
		i++;
		for (; i < len; ++i)
		{
			if (path[i] == '/')
				return 0;
		}
		return 1;
	} 
	if (recursive == 1)
	{
		for (i = 0; i < p_len; ++i)
		{
			if (path[i] == p_path[i])
				continue;
			if (path[i] != p_path[i])
				return 0;
		}
		if (path[i] != '/')
			return 0;
		return 1;
	}		
	return -1;
}


int init_pcheck_info(char *mount_path)
{
	return 0;
}

int set_root_path(struct pacon *pacon, char *r_path)
{
	int p_len = strlen(r_path);
	if (p_len >= MOUNT_PATH_MAX -1)
	{
		printf("set root path: root dir too long\n");
		return -1;
	}
	int i;
	for (i = 0; i < p_len; ++i)
	{
		pacon->mount_path[i] = r_path[i];	
	}
	pacon->mount_path[i] == '\0';
	printf("set root dir to %s\n", pacon->mount_path);
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
	//kv_handle = kv;
	pacon->kv_handle = kv;

	// inital permission check info
	ret = init_pcheck_info(pacon->mount_path);
	if (ret != 0)
	{
		printf("inital permission check info error\n");
		return -1;
	}

	int i;
	for (i = 0; i < MOUNT_PATH_MAX-1; ++i)
	{
		if (pacon->mount_path != '\0')
		{
			mount_path[i] = pacon->mount_path[i];
		} else {
			break;
		}
	}
	mount_path[i] = '\0';

	// init mq
	void *context = zmq_ctx_new();
    void *publisher = zmq_socket(context, ZMQ_PUB);
    int rc = zmq_bind(publisher, "ipc:///tmp/pacon_commit");
    if (rc != 0)
    {
    	printf("init zeromq error\n");
    	return -1;
    }
    pacon->publisher = publisher;
    pacon->context = context;
    
    // init the root dir of the consistent area
	cJSON *j_body;
	j_body = cJSON_CreateObject();
	cJSON_AddNumberToObject(j_body, "flags", 0);
	cJSON_AddNumberToObject(j_body, "mode", S_IFDIR | 0755);
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
	ret = dmkv_add(pacon->kv_handle, pacon->mount_path, value);
    if (ret != 0)
    {
    	printf("inital root dir fail\n");
    	return -1;
    }
	return 0;
}

int free_pacon(struct pacon *pacon)
{
	dmkv_free(pacon->kv_handle);
	return 0;
} 

/* only cache the result of parent check */
int add_to_dir_check_table(struct pacon *pacon, char *parent_dir)
{
	int ret;

	return 0;
}

/* 0 is hit, -1 is miss */
int dir_check_local(struct pacon *pacon, char *parent_dir)
{
	int ret;
	
	return 0;
}

/* 
 * check the parent dir if existed
 * GET will cause unnecessary data transmition, so we use CAS here
 * if parent dir existed, we add the check result into the pdir_check_table
 */
int check_parent_dir(struct pacon *pacon, char *path)
{
	char p_dir[PATH_MAX];
	int path_len = strlen(path), i;
	if (path_len == 1 && path[0] == '/')
		goto out;

	if (path_len >= PATH_MAX)
	{
		printf("mkdir: path too long\n");
		return -1;
	}
	for (i = path_len-1; i >= 0; --i)
	{
		if (path[i] == '/')
			break;
	}
	if (i == 0)
	{
		p_dir[0] = '/';
		p_dir[1] = '\0';
	} else {
		p_dir[i] = '\0';
	}

	for (i = i-1; i >= 0; --i)
	{
		p_dir[i] = path[i];
	}

	// check parent dir
	char *val;
	val = dmkv_get(pacon->kv_handle, p_dir);
	if (val == NULL)
		return -1;
	//add_to_dir_check_table(p_dir);
out:
	return 0;
}

int add_to_mq(struct pacon *pacon, char *path, char *opt_type)
{
	int path_len = strlen(path);
	if (path_len+3 > PATH_MAX)
	{
		printf("mq: path is too long\n");
		return -1;
	}
	char mesg[PATH_MAX];
	sprintf(mesg, "%s%s", path, opt_type);
	mesg[path_len+2] = '\0';
	zmq_send(pacon->publisher, mesg, strlen(mesg), 0);
	return 0;
}

/**************** file interfaces ***************/

int pacon_open(struct pacon *pacon, const char *path, int flags, mode_t mode, struct pacon_file *p_file)
{
	int ret;
	ret = child_cmp(path, mount_path, 1);
	if (ret != 1)
	{
		p_file->hit = 0;
		return open(path, flags, mode);
	}

	struct pacon_stat *st = (struct pacon_stat *)malloc(sizeof(struct pacon_stat));
	ret = pacon_getattr(pacon, path, st);
	if (ret == -1)
	{
		int fd = open(path, flags, mode);
		// cache in pacon
		int res;
		cJSON *j_body;
		j_body = cJSON_CreateObject();
		cJSON_AddNumberToObject(j_body, "flags", 0);
		cJSON_AddNumberToObject(j_body, "mode", S_IFDIR | 0755);
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
		res = dmkv_set(pacon->kv_handle, path, value);	
		p_file->fd = fd;
	}
	p_file->hit = 1;
	return 0;
}

int pacon_close(struct pacon *pacon, struct pacon_file *p_file)
{
	return 0;
}

int pacon_create(struct pacon *pacon, const char *path, mode_t mode)
{
	int ret;
	if (PARENT_CHECK == 1)
	{
		ret = check_parent_dir(pacon, path);
		if (ret != 0)
		{
			printf("create: parent dir not existed\n");
			return -1;
		}
	}
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
	ret = dmkv_add(pacon->kv_handle, path, value);
	if (ret != 0)
		return ret;
	ret = add_to_mq(pacon, path, CREATE);
	return ret;
}

int pacon_mkdir(struct pacon *pacon, const char *path, mode_t mode)
{
	int ret;
	if (PARENT_CHECK == 1)
	{
		ret = check_parent_dir(pacon, path);
		if (ret != 0)
		{
			printf("mkdir: parent dir not existed\n");
			return -1;
		}
	}
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
	ret = dmkv_add(pacon->kv_handle, path, value);
	if (ret != 0)
		return ret;
	ret = add_to_mq(pacon, path, MKDIR);
	return ret;
}

int pacon_getattr(struct pacon *pacon, const char* path, struct pacon_stat* st)
{
	char *val;
	val = dmkv_get(pacon->kv_handle, path);
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
	//j_fd = cJSON_GetObjectItem(j_body, "fd");
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
	//st->fd = (uint32_t)j_fd->valueint;
	//st->opt = (uint32_t)j_opt->valueint;
	return 0;
}

int pacon_rm(struct pacon *pacon, const char *path)
{
	int ret;
	ret = dmkv_del(pacon->kv_handle, path);
	return ret;
}

int pacon_rmdir(struct pacon *pacon, const char *path)
{
	int ret;
	ret = dmkv_del(pacon->kv_handle, path);
	return ret;
}

int pacon_read(struct pacon *pacon, const char *path, struct pacon_file *p_file, char *buf, size_t size, off_t offset)
{
	int ret;
	if (p_file->hit == 1 && size <= BUFFER_SIZE && offset == 0)
	{
		buf = p_file->buf;
		return 0;
	} 
	ret = read(p_file->fd, buf, size);
	return ret;
}

int pacon_write(struct pacon *pacon, const char *path, struct pacon_file *p_file, const char *buf, size_t size, off_t offset)
{
	return 0;
}	

int pacon_fsync(struct pacon *pacon, int fd)
{
	return 0;
}

/*
int pacon_opendir(struct pacon *pacon, const char *path)
{
	return fs_opendir(fs, path);
}

int pacon_readdir(struct pacon *pacon, const char *path, void *buf, off_t offset)
{
	// need implement a filler function
	return fs_readdir(fs, path, buf, filler, offset);
}

int pacon_rename(struct pacon *pacon, const char *path, const char *newpath)
{
	return fs_rename(fs, path, newpath);
}

int pacon_release(struct pacon *pacon, const char *path)
{
	 return fs_release(fs, path, file_info);
}

int pacon_releasedir(struct pacon *pacon, const char *path)
{
	return fs_releasedir(fs, path, file_info);
}

int pacon_utimens(struct pacon *pacon, const char * path, const struct timespec tv[2])
{
	return fs_utimens(fs, path, tv);
}

int pacon_truncate(struct pacon *pacon, const char * path, off_t offset)
{
	return fs_truncate(fs, path, offset);
}

int pacon_unlink(struct pacon *pacon, const char * path)
{
	return fs_unlink(fs, path);
}

int pacon_batch_chmod(struct pacon *pacon, const char * path, mode_t mode)
{
	return fs_chmod(fs, path, mode);
}

int pacon_batch_chown(struct pacon *pacon, const char * path, uid_t owner, gid_t group)
{
	return fs_chown(fs, path, owner, group);
}

int pacon_access(struct pacon *pacon, const char * path, int amode)
{
	return fs_access(fs, path, amode);
}

int pacon_symlink(struct pacon *pacon, const char * oldpath, const char * newpath)
{
	return fs_symlink(fs, oldpath, newpath);
}

int pacon_readlink(struct pacon *pacon, const char * path, char * buf, size_t size)
{
	return fs_readlink(fs, path, buf, size);
}
*/

