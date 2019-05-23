/*
 *  written by Yubo
 */
#include <stdio.h>
#include <malloc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h> 
#include <string.h>

#include "pacon.h"
#include "kv/dmkv.h"
#include "./lib/cJSON.h"
#include "./lib/cuckoo/hash_table.h"

#define SERI_TYPE 0 // 0 is memcpy, 1 is json
#define BUFFER_SIZE 64

// opt type
#define MKDIR ":1"
#define CREATE ":2"
#define RM ":3"
#define RMDIR ":4"
#define LINK ":5"

static struct dmkv *kv_handle;
static char mount_path[MOUNT_PATH_MAX];



/**************** get and set stat flags *****************/

enum statflags
{
	STAT_type,  // 0: dir, 1: file
	STAT_inline,  // 0: no inline data, 1: has inlien
	STAT_commit,  // 0: not yet be committed, 1: already be committed
	STAT_file_created,  // 0: not be created in DFS, 1: already be created in DFS
	STAT_existedin_dfs,  //  0: a new item after pacon run, 1: it was created before pacon run
	STAT_child_check,  //  0: haven't check its children, 1: already checked its children
};

void set_stat_flag(struct pacon_stat *p_st, int flag_type, int val)
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

int get_stat_flag(struct pacon_stat *p_st, int flag_type)
{
	if (((p_st->flags >> flag_type) & 1) == 1 )
	{
		return 1;
	} else {
		return 0;
	}
}

void set_fstat_flag(struct pacon_file *p_st, int flag_type, int val)
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

int get_fstat_flag(struct pacon_file *p_st, int flag_type)
{
	if (((p_st->flags >> flag_type) & 1) == 1 )
	{
		return 1;
	} else {
		return 0;
	}
}


/*********** serialization and deserialization ***********/

int seri_val(struct pacon_stat *s, char *val)
{
	//int size = sizeof(struct pacon_stat);
	memcpy(val, s, PSTAT_SIZE);
	return PSTAT_SIZE;
}

int deseri_val(struct pacon_stat *s, char *val)
{
	//int size = sizeof(struct pacon_stat);
	memcpy(s, val, PSTAT_SIZE);
	return 0;
}

// seri pacom_stat and inline_data to val
int seri_inline_data(struct pacon_stat *s, char *inline_data, char *val)
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
int deseri_inline_data(struct pacon_stat *s, char *inline_data, char *val)
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


/*********** parent dir check cache ***********/

/* only cache the result of parent check */
int add_to_dir_check_table(char *parent_dir)
{
	int flags = 0;
	pdirht_put(parent_dir, flags);
	return 0;
}

/* 0 is hit, -1 is miss */
int dir_check_local(char *parent_dir)
{
	int ret;
	ret = pdirht_get(parent_dir);
	return ret;
}


/************ some tool funcs ************/
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

int load_to_pacon(struct pacon *pacon, char *path)
{
	int ret;
	struct stat buf;
	ret = stat(path, &buf);
	if (ret != 0)
	{
		printf("root dir not existed on DFS\n");
		return -1;
	}
	if (SERI_TYPE == 0)
	{
		struct pacon_stat p_st;
		p_st.flags = 0;
		if ( S_ISDIR(buf.st_mode) )
		{
			// it is a dir
			set_stat_flag(&p_st, STAT_type, 0);
		} else {
			// it is a file
			set_stat_flag(&p_st, STAT_type, 1);
			set_stat_flag(&p_st, STAT_file_created, 1);
		}
		set_stat_flag(&p_st, STAT_existedin_dfs, 1);
		p_st.mode = buf.st_mode;
		p_st.ctime = buf.st_ctime;
		p_st.atime = buf.st_atime;
		p_st.mtime = buf.st_mtime;
		p_st.size = buf.st_size;
		p_st.uid = buf.st_uid;
		p_st.gid = buf.st_gid;
		p_st.nlink = buf.st_nlink;
		p_st.open_counter = 0;
		char val[PSTAT_SIZE];
		seri_val(&p_st, val);
		ret = dmkv_add(pacon->kv_handle, path, val, PSTAT_SIZE);
	} else{
		cJSON *j_body;
		j_body = cJSON_CreateObject();
		cJSON_AddNumberToObject(j_body, "flags", 0);
		cJSON_AddNumberToObject(j_body, "mode", buf.st_mode);
		cJSON_AddNumberToObject(j_body, "ctime", buf.st_ctime);
		cJSON_AddNumberToObject(j_body, "atime", buf.st_atime);
		cJSON_AddNumberToObject(j_body, "mtime", buf.st_mtime);
		cJSON_AddNumberToObject(j_body, "size", buf.st_size);
		cJSON_AddNumberToObject(j_body, "uid", buf.st_uid);
		cJSON_AddNumberToObject(j_body, "gid", buf.st_gid);
		cJSON_AddNumberToObject(j_body, "nlink", buf.st_nlink);
		//cJSON_AddNumberToObject(j_body, "opt", 0);
		//set_opt_flag(md, OP_mkdir, 1);
		char *value = cJSON_Print(j_body);
		ret = dmkv_add(pacon->kv_handle, path, value, PSTAT_SIZE);
	}
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
    //int q_len = 0;
    //int rc = zmq_setsockopt(publisher, ZMQ_SNDHWM, &q, sizeof(q_len));
    int rc = zmq_connect(publisher, "ipc:///run/pacon_commit");
    if (rc != 0)
    {
    	printf("init commit mq error\n");
    	return -1;
    }
    pacon->publisher = publisher;
    pacon->context = context;

    // init local rpc
	void *context_local_rpc = zmq_ctx_new();
    void *local_rpc_req = zmq_socket(context_local_rpc, ZMQ_REQ);
    //int q_len = 0;
    //int rc = zmq_setsockopt(publisher, ZMQ_SNDHWM, &q, sizeof(q_len));
    int rc = zmq_connect(local_rpc_req, "ipc:///run/pacon_local_rpc");
    if (rc != 0)
    {
    	printf("init local rpc error\n");
    	return -1;
    }
    pacon->local_rpc_req = local_rpc_req;
    pacon->context_local_rpc = context_local_rpc;
    
    // init the root dir of the consistent area
    // get the root dir stat from DFS
    ret = load_to_pacon(pacon, pacon->mount_path);
    if (ret != 0)
    {
    	printf("inital root dir fail\n");
    	return -1;
    }
    add_to_dir_check_table(pacon->mount_path);
	return 0;
}

int free_pacon(struct pacon *pacon)
{
	dmkv_free(pacon->kv_handle);
	zmq_close(pacon->publisher);
	zmq_ctx_destroy(pacon->context);
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

	int ret;
	// check parent dir on local
	ret = dir_check_local(p_dir);
	if (ret != -1)
		goto out;

	// check parent dir on dmkv
	char *val;
	uint64_t cas;
	uint64_t cas_temp;
	val = dmkv_get_cas(pacon->kv_handle, p_dir, &cas);
	if (val == NULL)
	{
		ret = load_to_pacon(pacon, p_dir);
		if (ret != 0)
		{
			printf("fail to load parent dir\n");
			return -1;
		}
		val = dmkv_get_cas(pacon->kv_handle, p_dir, &cas);
		cas_temp = cas;
	} else {
		cas_temp = cas;
	}
	// check its children
	struct pacon_stat p_st;
	deseri_val(&p_st, val);
	if (get_stat_flag(&p_st, STAT_type) == 1)
	{
		printf("check parent dir: parent dir is a file\n");
		return -1;
	}

	// if the p_dir's children are not be checked before, 
	// read dir and load its children into pacon
	if (get_stat_flag(&p_st, STAT_existedin_dfs) == 1 &&
		get_stat_flag(&p_st, STAT_child_check) == 0)
	{
		DIR *pd;
		struct dirent *entry;
		char child_path[PATH_MAX];
		int p_len = strlen(p_dir);
		memcpy(child_path, p_dir, p_len);
		child_path[p_len] = '/';
		pd = opendir(p_dir);
		while ( (entry = readdir(pd)) != NULL )
		{
			if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
				continue;
			if (p_len + strlen(entry->d_name) >= PATH_MAX-1)
			{
				printf("check parent dir: child path too long\n");
				return -1;
			}
			int c_len = strlen(entry->d_name);
			memcpy(child_path + p_len + 1, entry->d_name, c_len);
			child_path[p_len+1+c_len] = '\0';
			ret = load_to_pacon(pacon, child_path);
			if (ret != 0)
			{
				printf("check parent dir: fail to load child entry\n");
				return -1;
			}
		} 
		closedir(pd);

		set_stat_flag(&p_st, STAT_child_check, 1);
		seri_val(&p_st, val);
		ret = dmkv_cas(pacon->kv_handle, p_dir, val, PSTAT_SIZE, cas_temp);
		while (ret == 1)
		{
			val = dmkv_get_cas(pacon->kv_handle, p_dir, &cas);
			set_stat_flag(&p_st, STAT_child_check, 1);
			seri_val(&p_st, val);
			ret = dmkv_cas(pacon->kv_handle, p_dir, val, PSTAT_SIZE, cas);
		}
		if (ret == -1)
		{
			printf("check parent dir: fail to update STAT_child_check flag\n");
			return -1;
		}
	}

	add_to_dir_check_table(p_dir);
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

int local_fd_open(char *path)
{
	int ret;
	ret = fdht_get(path);
	return ret;
}

int add_local_fd(char *path, int fd)
{
	int ret;
	ret = fdht_put(path, fd);
	ret = opc_put(path, 1);
	return ret;
}

struct pacon_file * new_pacon_file(void)
{
	struct pacon_file *p_file = (struct pacon_file *)malloc(sizeof(struct pacon_file));
	//char *buf = (char *)malloc(INLINE_MAX);
	//p_file->buf = buf;
	return p_file;
}

void free_pacon_file(struct pacon_file *p_file)
{
	//free(p_file->buf);
	//free(p_file);
	//p_file = NULL;
}

/**************** file interfaces ***************/

/* 
 * 1. search in local fd table, pacon and DFS
 * 2. get its fd and increase the open counter ()
 * 3. get inline data if necessary
 */
int pacon_open(struct pacon *pacon, const char *path, int flags, mode_t mode, struct pacon_file *p_file)
{
	int ret;
	/*ret = child_cmp(path, mount_path, 1);
	if (ret != 1)
	{
		p_file->hit = 0;
		return open(path, flags, mode);
	}*/

	/* check fd in local fd table, if fd existed, goto out
	ret = local_fd_open(path);
	if (ret >= 0)
	{
		p_file->fd = ret;
		goto out;
	}*/
	p_file->open_flags = flags;
	p_file->open_mode = mode;
	char *val;
	uint64_t cas;

retry:
	val = dmkv_get_cas(pacon->kv_handle, path, &cas);
	// not be cached
	if (val == NULL)
	{
		struct stat buf;
		int st_ret = stat(path, &buf);
		int fd = open(path, flags, mode);
		if (fd == -1 || st_ret != 0)
		{
			printf("file is not existed\n");
			return -1;
		}
		// cache in pacon
		int res;
		if (SERI_TYPE == 0)
		{
			struct pacon_stat p_st;
			p_st.flags = 0;
			set_stat_flag(&p_st, STAT_file_created, 1);
			p_st.mode = buf.st_mode;
			p_st.ctime = buf.st_ctime;
			p_st.atime = buf.st_atime;
			p_st.mtime = buf.st_mtime;
			p_st.size = buf.st_size;
			p_st.uid = buf.st_uid;
			p_st.gid = buf.st_gid;
			p_st.nlink = buf.st_nlink;
			p_st.open_counter++;  // only be used when file was created in DFS
			char val[PSTAT_SIZE];
			seri_val(&p_st, val);
			res = dmkv_add(pacon->kv_handle, path, val, PSTAT_SIZE);
			if (res != 0)
				goto retry;
			p_file->fd = fd;
		} else {
			/*cJSON *j_body;
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
			res = dmkv_add(pacon->kv_handle, path, value, PSTAT_SIZE);*/
		}

		// add fd to local fd table
		ret = add_local_fd(path, fd);
	} else {
		struct pacon_stat st;
		deseri_val(&st, val);

		// file md had been cached but not yet been opened
		if (get_stat_flag(&st, STAT_file_created) == 1 && 
			get_stat_flag(&st, STAT_inline) == 0 &&
			st.size > 0)
		{
			int fd = open(path, flags, mode);
			if (fd == -1)
			{
				printf("file is not existed\n");
				return -1;
			}
			while (1)
			{
				st.open_counter++;
				seri_val(&st, val);
				ret = dmkv_cas(pacon->kv_handle, path, val, PSTAT_SIZE, cas);
				if (ret == 1)
				{
					val = dmkv_get_cas(pacon->kv_handle, path, &cas);
					deseri_val(&st, val);
					continue;
				}
				if (ret == -1)
				{
					printf("open a cached file error\n");
					return -1;
				}
				if (ret == 0)
				{
					break;
				}
			}
			ret = add_local_fd(path, fd);
			p_file->fd = fd;
		} else {
			p_file->fd = -1;
		}
	}
out:
	return 0;
}

/* reduce the opened counter */
int pacon_close(struct pacon *pacon, struct pacon_file *p_file)
{
	//free_pacon_file(p_file);
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
	if (SERI_TYPE == 0)
	{
		struct pacon_stat p_st;
		p_st.flags = 0;
		set_stat_flag(&p_st, STAT_type, 1);
		p_st.mode = mode;
		p_st.ctime = time(NULL);
		p_st.atime = time(NULL);
		p_st.mtime = time(NULL);
		p_st.size = 0;
		p_st.uid = getuid();
		p_st.gid = getgid();
		p_st.nlink = 0;
		p_st.open_counter = 0;
		char val[PSTAT_SIZE];
		seri_val(&p_st, val);
		ret = dmkv_add(pacon->kv_handle, path, val, PSTAT_SIZE);
	} else {
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
		ret = dmkv_add(pacon->kv_handle, path, value, PSTAT_SIZE);
	}
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
	if (SERI_TYPE == 0)
	{
		struct pacon_stat p_st;
		p_st.flags = 0;
		p_st.mode = mode;
		p_st.ctime = time(NULL);
		p_st.atime = time(NULL);
		p_st.mtime = time(NULL);
		p_st.size = 0;
		p_st.uid = getuid();
		p_st.gid = getgid();
		p_st.nlink = 0;
		p_st.open_counter = 0;
		char val[PSTAT_SIZE];
		seri_val(&p_st, val);
		ret = dmkv_add(pacon->kv_handle, path, val, PSTAT_SIZE);
	} else {
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
		ret = dmkv_add(pacon->kv_handle, path, value, PSTAT_SIZE);
	}
	if (ret != 0)
		return ret;
	ret = add_to_mq(pacon, path, MKDIR);
	return ret;
}

int pacon_getattr(struct pacon *pacon, const char* path, struct pacon_stat* st)
{
	char *val;
	val = dmkv_get(pacon->kv_handle, path);
	// if not in pacon, try to get from DFS
	if (val == NULL)
	{
		int ret;
		struct stat buf;
		ret = stat(path, &buf);
		if (ret != 0)
		{
			printf("getattr: path not existed: %s\n", path);
			return -1;
		}
		st->flags = 0;
		st->mode = buf.st_mode;
		st->ctime = buf.st_ctime;
		st->atime = buf.st_atime;
		st->mtime = buf.st_mtime;
		st->size = buf.st_size;
		st->uid = buf.st_uid;
		st->gid = buf.st_gid;
		st->nlink = buf.st_nlink;
		st->open_counter = 0;
		char val[PSTAT_SIZE];
		seri_val(st, val);
		ret = dmkv_add(pacon->kv_handle, path, val, PSTAT_SIZE);
		goto out;
	}

	// md in pacon, deserialize it
	if (SERI_TYPE == 0)
	{
		deseri_val(st, val);
		if (get_stat_flag(st, STAT_inline) == 0 &&
			get_stat_flag(st, STAT_file_created) == 1)
		{
			int ret;
			struct stat buf;
			ret = stat(path, &buf);
			st->ctime = buf.st_ctime;
			st->atime = buf.st_atime;
			st->mtime = buf.st_mtime;
			st->size = buf.st_size;
		}
	} else {
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
	}
out:
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

/* success: return read bytes, error: return -1 */
int pacon_read(struct pacon *pacon, char *path, struct pacon_file *p_file, char *buf, size_t size, off_t offset)
{
	int ret;
	if (p_file->size == 0)
	{
		buf = NULL;
		return 0;
	}

	// DFS case
	if (p_file->fd != -1)
	{
		ret = pread(p_file->fd, buf, size, offset);
		return ret;
	} else {
	// inline case
		struct pacon_stat new_st;
		char *val;
		uint64_t cas;
		val = dmkv_get_cas(pacon->kv_handle, path, &cas);
		if (val == NULL)
		{
			printf("read: get inline data error\n");
			return -1;
		}
		char inline_data[INLINE_MAX];
		deseri_inline_data(&new_st, inline_data, val);

		// if in DFS, open it and 
		if (get_stat_flag(&new_st, STAT_inline) == 0 &&
			get_stat_flag(&new_st, STAT_file_created) == 1 &&
			new_st.size >0)
		{
			int fd = open(path, p_file->flags, p_file->mode);
			if (fd == -1)
			{
				printf("file is not existed\n");
				return -1;
			}
			while (1)
			{
				new_st.open_counter++;
				seri_val(&new_st, val);
				ret = dmkv_cas(pacon->kv_handle, path, val, PSTAT_SIZE, cas);
				if (ret == 1)
				{
					val = dmkv_get_cas(pacon->kv_handle, path, &cas);
					deseri_val(&new_st, val);
					continue;
				}
				if (ret == -1)
				{
					printf("open a cached file error\n");
					return -1;
				}
				if (ret == 0)
				{
					break;
				}
			}
			ret = add_local_fd(path, fd);
			p_file->fd = fd;
			ret = pread(p_file->fd, buf, size, offset);
			return ret;
		}

		if (offset + size > new_st.size)
		{
			printf("read overflow\n");
			return -1;
		}
		memcpy(buf, inline_data+offset, size);
		buf[size] = '\0';
		return size;
	}
	return -1;
}

/* success: return write bytes, error: return -1 */
int pacon_write(struct pacon *pacon, char *path, struct pacon_file *p_file, const char *buf, size_t size, off_t offset)
{
	int ret;
	int data_size = strlen(buf);
	if (data_size > size)
	{
		printf("write buffer size error\n");
		return -1;
	}
begin:
	// DFS case
	if (p_file->fd != -1)
	{
		ret = pwrite(p_file->fd, buf, size, offset);
		return ret;
	}

	char *val;
	uint64_t cas;
	struct pacon_stat new_st;
	char inline_data[INLINE_MAX];

retry:
	val = dmkv_get_cas(pacon->kv_handle, path, &cas);
	deseri_inline_data(&new_st, inline_data, val);

	// empty file and not been created in DFS
	if (get_stat_flag(&new_st, STAT_inline) == 0 && get_stat_flag(&new_st, STAT_file_created) == 0)
	{
		if (size + offset < INLINE_MAX - 1)
		{
			new_st.flags = p_file->flags;
			set_stat_flag(&new_st, STAT_inline, 1);
			new_st.atime = time(NULL);
			new_st.mtime = time(NULL);
			new_st.size = size + offset;
			char val[PSTAT_SIZE+INLINE_MAX];
			seri_inline_data(&new_st, buf, val);
			ret = dmkv_cas(pacon->kv_handle, path, val, PSTAT_SIZE + size, cas);
			// conflict, retry it
			if (ret == 1)
				goto retry;
			
			if (ret == -1)
			{
				printf("write inline data error\n");
				return -1;
			}
			p_file->size = size + offset;
			return size;
		} else {
			printf("need buffer outside the md\n");
			return -1;
		}
	}

	// inline case & buffer in the metadata
	if (get_stat_flag(&new_st, STAT_inline) == 1 && get_stat_flag(&new_st, STAT_file_created) == 0)
	{
		int new_size = ((new_st.size-offset)+size)>new_st.size?((new_st.size-offset)+size):new_st.size;
		if (new_size < INLINE_MAX - 1)
		{
			char new_data[INLINE_MAX];
			memcpy(new_data, inline_data, offset);
			memcpy(new_data+offset, buf, size);
			if (offset + size < new_st.size)
				memcpy(new_data+offset+size, inline_data+offset+size, new_st.size-(offset+size));
			new_data[new_size+1] = '\0';
			new_st.atime = time(NULL);
			new_st.mtime = time(NULL);
			new_st.size = new_size;
			char val[PSTAT_SIZE+INLINE_MAX];
			seri_inline_data(&new_st, &new_data, val);
			ret = dmkv_cas(pacon->kv_handle, path, val, PSTAT_SIZE + new_size, cas);
			// conflict, retry it
			if (ret == 1)
				goto retry;

			if (ret == -1)
			{
				printf("write inline data error\n");
				return -1;
			}
			p_file->size = size + offset;
			return size;
		} else {
			printf("need buffer outside the md\n");
			return -1;
		}
	}

	/* inline case & buffer outside the metadata
	if (p_file->buf == NULL)
	{
		
	}*/

	// DFS case
	if (get_stat_flag(&new_st, STAT_inline) == 0 && get_stat_flag(&new_st, STAT_file_created) == 1)
	{
		int fd = open(path, p_file->flags, p_file->mode);
		if (fd == -1)
		{
			printf("file is not existed\n");
			return -1;
		}
		while (1)
		{
			new_st.open_counter++;
			seri_val(&new_st, val);
			ret = dmkv_cas(pacon->kv_handle, path, val, PSTAT_SIZE, cas);
			if (ret == 1)
			{
				val = dmkv_get_cas(pacon->kv_handle, path, &cas);
				deseri_val(&new_st, val);
				continue;
			}
			if (ret == -1)
			{
				printf("open a cached file error\n");
				return -1;
			}
			if (ret == 0)
			{
				break;
			}
		}
		ret = add_local_fd(path, fd);
		p_file->fd = fd;
		goto begin;
	}
	return -1;
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

