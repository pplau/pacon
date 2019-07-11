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

// opt type for commi
#define CHECKPOINT ":0"
#define MKDIR ":1"
#define CREATE ":2"
#define RM ":3"
#define RMDIR ":4"
#define READDIR ":5"
#define OWRITE ":6"  // data size is larger than the INLINE_MAX, write it back to DFS
#define FSYNC ":7"
#define RENAME ":A"
#define FLUSHDIR ":B"

// opt type for permission check
#define READDIR_PC 0
#define MKDIR_PC 1
#define CREATE_PC 2
#define RM_PC 3
#define RMDIR_PC 4
#define LINK_PC 5
#define OWRITE_PC 6  // data size is larger than the INLINE_MAX, write it back to DFS
#define FSYNC_PC 7
#define READ_PC 8
#define WRITE_PC 9
#define RW_PC 10
#define CREATEW_PC 11
#define RENAME_PC 12


#define DEFAULT_FILEMODE S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH
#define DEFAULT_DIRMORE S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IXOTH

#define SECTOR_SIZE 512
#define TIMESTAMP_SIZE 11

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
	STAT_rm,
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
 * ret = 1 means recursive sub file/dir, 0 means not recursive sub file/dir
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

/* 
 * ret = 1 means it is sub file/dir, 0 means it is not sub file/dir, 2 mean they are the same path
 */
int child_cmp_new(char *path, char *p_path, int recursive)
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

/* similar to rmdir, but do not need to remove dir on DFS */
int flush_dir(struct pacon *pacon, char *path)
{
	int ret;
	add_to_mq(pacon, path, FLUSHDIR, time(NULL));
	ret = add_to_local_rpc(pacon, path, FLUSHDIR, time(NULL));
	if (ret == -1)
	{
		printf("flush dir error: %s\n", path);
		return -1;
	}
	return ret;
}

int flush_file(struct pacon *pacon, char *path)
{
	int ret;
	struct stat buf;
	while (stat(path, &buf) != 0)
	ret = dmkv_del(pacon->kv_handle, path);
	if (ret != 0)
	{
		printf("flush file error\n");
		return -1;
	}
	return 0;
}

/*
 * Policy:
 * 1. 
 */
int evict_metadata(struct pacon *pacon)
{
	int ret;
	// get evict lock
	char *val;
	uint64_t cas;
	uint64_t cas_temp;
	char *evict_lock = "evict_lock";
retry:
	val = dmkv_get_cas(pacon->kv_handle, evict_lock, &cas);
	if (val == NULL)
	{
		printf("get evict lock error: not existed\n");
		return -1;
	}
	if (val[0] == '0')
	{
		ret = dmkv_cas(pacon->kv_handle, evict_lock, "1", strlen("1"), cas);
		if (ret == 1)
			goto retry;
	} else if (val[0] == '1') {
		goto retry;
	} else {
		printf("evict lock val error\n");
		return -1;
	}

	// find a entry to be evicted
	char *record_val;
	char *evict_record = "evict_record";
	record_val = dmkv_get(pacon->kv_handle, evict_record);
	if (record_val == NULL)
	{
		printf("get evict record val error\n");
		return -1;
	}
	int rr = atoi(record_val);
	int c;

	DIR *dir;
	struct dirent *entry;

find_again:
	dir = pacon_opendir(pacon, pacon->mount_path);
	entry = pacon_readdir(dir);
	c = 0;
	int found = 0;
	while (entry != NULL)
	{
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
		{
			entry = pacon_readdir(dir);
			continue;
		}
		if (c == rr)
		{
			char evict_path[PATH_MAX];
			sprintf(evict_path, "%s%s%s", pacon->mount_path, "/", entry->d_name);
			if (entry->d_type == DT_DIR)
			{
				ret = flush_dir(pacon, evict_path);
				if (ret != 0 )
				{
					printf("evict metadata: flush dir error\n");
					return -1;
				}
			} else {
				ret = flush_file(pacon, evict_path);
				if (ret != 0)
				{
					printf("evict metadata: flush file error\n");
					return -1;
				}
			}
			rr++;
			found = 1;
			break;
		}
		c++;
		entry = pacon_readdir(dir);
	}
	pacon_closedir(pacon, dir);
	if (found == 0)
	{
		rr = 0;
		goto find_again;
	}

	char new_evict_record[128];
	sprintf(new_evict_record, "%d", rr);
	ret = dmkv_set(pacon->kv_handle, evict_record, new_evict_record, strlen(new_evict_record));
	if (ret != 0)
	{
		printf("update evict record error\n");
		return -1;
	}

	// free evict lock, doesn't need cas, becasue only the item has already been locked
	ret = dmkv_set(pacon->kv_handle, evict_lock, "0", strlen("0"));
	if (ret != 0)
	{
		printf("free evict lock error\n");
		return -1;
	}
	return 0;
}

int load_to_pacon(struct pacon *pacon, char *path)
{
	int ret;
	ret = child_cmp_new(path, pacon->mount_path, 1);
	if (ret == 0)
	{
		printf("path doesn't belong to the workspace\n");
		return -1;
	}

	struct stat buf;
	ret = stat(path, &buf);
	if (ret != 0)
	{
		printf("load to pacon: target dir not existed on DFS\n");
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
evict_ok:
		ret = dmkv_add(pacon->kv_handle, path, val, PSTAT_SIZE);
		// memory is insufficient
		if (ret == -2)
		{
			evict_metadata(pacon);
			goto evict_ok;
		}
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

void get_local_addr(char *ip)
{
	FILE *fp;
	fp = fopen("./local_config", "r");
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
    rc = zmq_connect(local_rpc_req, "tcp://127.0.0.1:5555");
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

    // init fsync log: 1. check the path  2. create the log file
    if (access(FSYNC_LOG_PATH, F_OK) != 0)
    {
    	if (mkdir(FSYNC_LOG_PATH, DEFAULT_DIRMORE) != 0)
    	{
    		printf("create fsync log dir error\n");
    		return -1;
    	}
    }
    //char fsync_logfile_path[128];
    char local_ip[17];
    get_local_addr(local_ip);
    int pid = (int)getpid();
    char pid_s[45];
    sprintf(pid_s, "%d", pid);
    char filename[64];
    strcpy(filename, local_ip);
    filename[strlen(local_ip)] = '.';
    strcpy(filename + strlen(local_ip) + 1, pid_s);
    strcpy(pacon->fsync_logfile_path, FSYNC_LOG_PATH);
    strcpy(pacon->fsync_logfile_path + strlen(FSYNC_LOG_PATH), filename);
    int fd;
    fd = open(pacon->fsync_logfile_path, O_RDWR | O_DIRECT);
    if (fd == -1) 
    {
	    fd = creat(pacon->fsync_logfile_path, DEFAULT_FILEMODE);
	    if (fd == -1)
	    {
	    	printf("create fsync log file error\n");
	    	return -1;
	    }
    }
    pacon->fsync_log_fd = fd;
    pacon->log_size = 0;
    // init permission info
    pacon->perm_info = NULL;
    pacon->df_dir_mode =  S_IFDIR | 0755;
    pacon->df_f_mode = S_IFREG | 0644;
    // init joint cr info
    pacon->cr_num = 0;

    // init evict lock, 0 is unlock, 1 is lock
    char *evict_lock = "evict_lock";
    char *lock_val = "0";
    ret = dmkv_add(pacon->kv_handle, evict_lock, lock_val, strlen(evict_lock));
    /*if (ret != 0)
    {
    	printf("init evict lock error\n");
    	return -1;
    }*/
    char *evict_record = "evict_record";
    char *record_val = "?";
    ret = dmkv_add(pacon->kv_handle, evict_record, record_val, strlen(record_val));
    /*if (ret != 0)
    {
    	printf("init evict record error\n");
    	return -1;
    }*/

    // init share memory, for async rpc
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
    	shm = shmat(shmid,  (void*)0, 0);
    	if (shm == (void*) -1)
    	{
    		printf("shmat error\n");
    		return -1;
    	}
    	pacon->rmdir_record = (struct rmdir_record *)shm;
    	pacon->rmdir_record->rmdir_num = 0;
    	pacon->rmdir_record->shmid_count = 1;
    }
	return 0;
}

int free_pacon(struct pacon *pacon)
{
	int ret;
	dmkv_free(pacon->kv_handle);
	zmq_close(pacon->publisher);
	zmq_ctx_destroy(pacon->context);
	zmq_close(pacon->local_rpc_req);
	zmq_ctx_destroy(pacon->context_local_rpc);

	if (ASYNC_RPC == 1)
	{
		ret = shmdt(pacon->rmdir_record);
		if (ret == -1)
		{
			printf("free share memory error\n");
			return -1;
		}
	}
	return 0;
} 

// 0: not in rmdir list, -1: in rmdir list
int check_rmdir_list(struct pacon *pacon, char *path)
{
	int i;
	struct rmdir_record *rmdir_record;
	for (i = 1; i <= pacon->rmdir_record->shmid_count; ++i)
	{
		if (i == 1)
		{
			rmdir_record = pacon->rmdir_record;
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

		int j, pos;
		for (j = 0; j < pacon->rmdir_record->rmdir_num; ++j)
		{
			pos = j - (RMDIRLIST_MAX * (i-1));
			if (child_cmp_new(path, rmdir_record->rmdir_list[i], 1) != 0)
				return -1;
		}
	}
	return 0;
}

/* 
 * check the parent dir if existed
 * GET will cause unnecessary data transmition, so we use CAS here
 * if parent dir existed, we add the check result into the pdir_check_table
 */
int check_parent_dir(struct pacon *pacon, char *path)
{
	int ret;
	char p_dir[PATH_MAX];
	int path_len = strlen(path), i;
	if (path_len == 1 && path[0] == '/')
		goto out;

	if (path[path_len-1] == '/')
		path_len--;

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

	// check in rmdir list
	if (ASYNC_RPC == 1)
	{
		if (pacon->rmdir_record->rmdir_num > 0)
		{
			ret = check_rmdir_list(pacon, p_dir);
			if (ret != 0)
			{
				printf("parent is removed\n");
				return -1;
			}
		}
	}

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
			deseri_val(&p_st, val);
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

int add_to_mq(struct pacon *pacon, char *path, char *opt_type, uint32_t timestamp)
{
	int path_len = strlen(path);
	if (path_len+3 > PATH_MAX)
	{
		printf("mq: path is too long\n");
		return -1;
	}
	char mesg[PATH_MAX+TIMESTAMP_SIZE];
	//sprintf(mesg, "%s%s", path, opt_type);
	// add timestamp
	char ts[TIMESTAMP_SIZE];
	sprintf(ts, "%d", timestamp);
	sprintf(mesg, "%s%s%s", path, opt_type, ts);
	//mesg[path_len+2] = '\0';
	zmq_send(pacon->publisher, mesg, strlen(mesg), 0);
	return 0;
}

int add_to_local_rpc(struct pacon *pacon, char *path, char *opt_type, uint32_t timestamp)
{
	int path_len = strlen(path);
	if (path_len+3 > PATH_MAX)
	{
		printf("mq: path is too long\n");
		return -1;
	}
	char mesg[PATH_MAX];
	char ts[TIMESTAMP_SIZE];
	sprintf(ts, "%d", timestamp);
	sprintf(mesg, "%s%s%s", path, opt_type, ts);
	//mesg[path_len+2] = '\0';
	char rep[2];
	zmq_send(pacon->local_rpc_req, mesg, strlen(mesg), 0);
	zmq_recv(pacon->local_rpc_req, rep, 1, 0);

	// return 0 means success, 1 means error
	if (rep[0] == '0')
		return 0;
	else
		return -1;
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

/* type = 0 is dir, = 1 is file */
int mask_compare(mode_t perm_mode, int type, int opt)
{
	char mode[5];
	sprintf(mode, "%d", perm_mode);
	switch (mode[0])
	{
		case '7':
			return 0;

		case '6':
			if (opt != READDIR_PC ||
				opt != READ_PC ||
				opt != WRITE_PC ||
				opt != RW_PC)
				return -1;
			return 0;

		case '5':
			if (opt != READDIR_PC || 
				opt != READ_PC)
				return -1;
			return 0;

		case '4':
			if (opt != READ_PC)
				return -1;
			return 0;

		case '3':
			if (opt != MKDIR_PC ||
				opt != CREATE_PC ||
				opt != RM_PC ||
				opt != RMDIR_PC ||
				opt != WRITE_PC ||
				opt != CREATEW_PC ||
				opt != RENAME_PC)
				return -1;
			return 0;

		case '2':
			if (opt != WRITE_PC)
				return -1;
			return 0;

		case '1':
			return -1;

		case '0':
			return -1;
	}
}

/* 
 * type = 0 is dir, = 1 is file
 * return 0 is legal for normal, i is legal for spcial location, -1 is illegal 
 */
int check_permission(struct pacon *pacon, char *path, int type, int opt)
{
	int ret;
	int i;
	int nor_or_sp = 0; // 0 is nor, 1 is sp

	// if opt is create/remove file, check its parent first
	/*if (type == 1)
	{
		if (opt == CREATE_PC || opt == CREATEW_PC || opt == RM_PC)
		{
			char p_path[PATH_MAX];
			int len = strlen(path);
			for (i = len-1; i >= 0; --i)
			{
				if (path[i] == '/')
					break;
			}
			strncpy(p_path, path, i);
			p_path[i] = '\0';
			ret = check_permission(pacon, p_path, 0, opt);
			if (ret != 0)
				return -1;
		}
	}*/

	int type_tmp;
	char check_path[PATH_MAX];
	int len = strlen(path);

	// if opt is create / mkdir (the md doesn't exist before), check its parent
	if (opt == CREATE_PC || opt == CREATEW_PC || opt == MKDIR_PC)
	{
		for (i = len-1; i >= 0; --i)
		{
			if (path[i] == '/')
				break;
		}
		strncpy(check_path, path, i);
		check_path[i] = '\0';
		type_tmp = 0;
	} else {
		strncpy(check_path, path, len);
		check_path[len] = '\0';
		type_tmp = type_tmp;
	}

	if (type_tmp == 0)
	{
		// check normal permission
		ret = mask_compare(pacon->perm_info->nom_dir_mode, type, opt);
		if (ret == -1)
		{
			printf("pacon dir check: permission denied %s\n", path);
			return -1;
		}

		// check special permission
		for (i = 0; i < pacon->perm_info->sp_num; ++i)
		{
			ret = child_cmp_new(check_path, pacon->perm_info->sp_path[i], 1);
			if (ret > 0)
			{
				if (ret == 2)
					nor_or_sp = i;
				ret = mask_compare(pacon->perm_info->sp_dir_modes[i], type, opt);
				if (ret == -1)
				{
					printf("pacon dir check: permission denied %s\n", path);
					return -1;
				}
			}
		}
		if (nor_or_sp > 0)
			return nor_or_sp;
		else
			return 0;
	} else {
		// check normal permission
		ret = mask_compare(pacon->perm_info->nom_f_mode, type, opt);
		if (ret == -1)
		{
			printf("pacon file check: permission denied %s\n", path);
			return -1;
		}

		// check special permission
		for (i = 0; i < pacon->perm_info->sp_num; ++i)
		{
			ret = child_cmp_new(check_path, pacon->perm_info->sp_path[i], 1);
			if (ret > 0)
			{
				if (ret == 2)
					nor_or_sp = i;
				ret = mask_compare(pacon->perm_info->sp_f_modes[i], type, opt);
				if (ret == -1)
				{
					printf("pacon file check: permission denied %s\n", path);
					return -1;
				}
			}
		}
		if (nor_or_sp > 0)
			return nor_or_sp;
		else
			return 0;
	}
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
	if (pacon->perm_info != NULL)
	{
		ret = check_permission(pacon, path, 1, RW_PC);
		if (ret < 0)
			return -1;
	}
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
	p_file->hit_remote_cr = 0;
	char *val;
	uint64_t cas;

retry:
	val = dmkv_get_cas(pacon->kv_handle, path, &cas);
	// not be cached
	if (val == NULL)
	{
		// remote cregion begin 
		ret = child_cmp_new(path, pacon->mount_path, 1);
		if (ret == 0)
		{
			if (pacon->cr_num == 0)
				return -1;
			int cr;
			for (cr = 0; cr < pacon->cr_num; ++cr)
			{
				ret = child_cmp_new(path, pacon->remote_cr_root[cr], 1);
				if (ret > 0)
				{
					ret = pacon_open(pacon->remote_pacon_list[cr], path, flags, mode, p_file);
					if (ret != 0)
					{
						printf("open: path not existed in remote cregions: %s\n", path);
						return -1;
					} else {
						p_file->hit_remote_cr = 1;
						return 0;
					}
				}
			}
			printf("open: path not existed in remote cregions: %s\n", path);
			return -1;	
		}
		// remote cregion end 

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
evict_ok:
			res = dmkv_add(pacon->kv_handle, path, val, PSTAT_SIZE);
			if (res != 0)
			{
				if (res == -2)
				{
					evict_metadata(pacon);
					goto evict_ok;
				}
				goto retry;
			}
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
	int f_mode;
	f_mode = pacon->df_f_mode;

	if (pacon->perm_info != NULL)
	{
		ret = check_permission(pacon, path, 1, CREATE_PC);
		if (ret < 0)
			return -1;
		f_mode = pacon->perm_info->sp_f_modes[ret];
	}

	uint32_t timestamp = time(NULL);
	if (PARENT_CHECK == 1)
	{
		ret = check_parent_dir(pacon, path);
		if (ret != 0)
		{
			printf("create: parent dir not existed\n");
			return -1;
		}
	}

	char val[PSTAT_SIZE];
	if (SERI_TYPE == 0)
	{
		struct pacon_stat p_st;
		p_st.flags = 0;
		set_stat_flag(&p_st, STAT_type, 1);
		p_st.mode = f_mode;
		p_st.ctime = timestamp;
		p_st.atime = timestamp;
		p_st.mtime = timestamp;
		p_st.size = 0;
		p_st.uid = getuid();
		p_st.gid = getgid();
		p_st.nlink = 0;
		p_st.open_counter = 0;
		//char val[PSTAT_SIZE];
		seri_val(&p_st, val);
evict_ok:
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

	// the file may be existed or be removed
	if (ret != 0)
	{
		if (ret == -1)
		{
			if (ASYNC_RPC == 1)
			{
				if (pacon->rmdir_record->rmdir_num > 0)
				{
					int ret1 = check_rmdir_list(pacon, path);
					if (ret1 == -1)
					{
						dmkv_set(pacon->kv_handle, path, val, PSTAT_SIZE);
						goto out;
					}
				}
			}
		}
		if (ret == -2)
		{
			evict_metadata(pacon);
			goto evict_ok;
		}

		char *tmp_val;
		struct pacon_stat tmp_p_st;
		tmp_val = dmkv_get(pacon->kv_handle, path);
		deseri_val(&tmp_p_st, tmp_val);
		if (get_stat_flag(&tmp_p_st, STAT_rm) == 1)
		{
			ret = dmkv_set(pacon->kv_handle, path, val, PSTAT_SIZE);
			if (ret != 0)
			{
				printf("fail to create file: %s\n", path);
				return -1;
			}
		} else {
			printf("file is existed\n");
			ret = -1;
		}
		//return ret;
	}
out:
	//ret = add_to_mq(pacon, path, CREATE);
	ret = add_to_mq(pacon, path, CREATE, timestamp);
	return ret;
}

/*
 * This func is used to fast write after create
 * It combine create, open, write and close together 
 * offset is 0 becase the file should be an empty file
 *
 * ONLY THIS FILE DATA FUNC DOES NOT NEED TO OPEN FILE !!
 */
int pacon_create_write(struct pacon *pacon, const char *path, mode_t mode, const char *buf, size_t size, struct pacon_file *p_file)
{
	int ret;
	int f_mode;
	f_mode = pacon->df_f_mode;

	if (pacon->perm_info != NULL)
	{
		ret = check_permission(pacon, path, 1, CREATE_PC);
		if (ret < 0)
			return -1;
		f_mode = pacon->perm_info->sp_f_modes[ret];
	}

	if (PARENT_CHECK == 1)
	{
		ret = check_parent_dir(pacon, path);
		if (ret != 0)
		{
			printf("create: parent dir not existed\n");
			return -1;
		}
	}

	struct pacon_stat p_st;
	if (size < INLINE_MAX - 1)
	{
		//struct pacon_stat p_st;
		p_st.flags = 0;
		set_stat_flag(&p_st, STAT_type, 1);
		set_stat_flag(&p_st, STAT_inline, 1);
		p_st.mode = f_mode;
		p_st.ctime = time(NULL);
		p_st.atime = time(NULL);
		p_st.mtime = time(NULL);
		p_st.size = size;
		p_st.uid = getuid();
		p_st.gid = getgid();
		p_st.nlink = 0;
		p_st.open_counter = 0;
		char val[PSTAT_SIZE+INLINE_MAX];
		seri_inline_data(&p_st, buf, val);
evict_ok:
		ret = dmkv_add(pacon->kv_handle, path, val, PSTAT_SIZE + size);

		// the file may be existed or be removed
		if (ret != 0)
		{
			if (ret == -1)
			{
				if (ASYNC_RPC == 1)
				{
					if (pacon->rmdir_record->rmdir_num > 0)
					{
						int ret1 = check_rmdir_list(pacon, path);
						if (ret1 == -1)
						{
							dmkv_set(pacon->kv_handle, path, val, PSTAT_SIZE);
							goto addmq;
						}
					}
				}
			}
			if (ret == -2)
			{
				evict_metadata(pacon);
				goto evict_ok;
			}

			char *tmp_val;
			struct pacon_stat tmp_p_st;
			tmp_val = dmkv_get(pacon->kv_handle, path);
			deseri_val(&tmp_p_st, tmp_val);
			if (get_stat_flag(&tmp_p_st, STAT_rm) == 1)
			{
				ret = dmkv_set(pacon->kv_handle, path, val, PSTAT_SIZE + size);
				if (ret != 0)
				{
					printf("fail to create file: %s\n", path);
					return -1;
				}
			} else {
				printf("file is existed\n");
				ret = -1;
			}
			//return ret;
		}
addmq:
		//ret = add_to_mq(pacon, path, CREATE);
		ret = add_to_mq(pacon, path, CREATE, p_st.ctime);
	} else {
		printf("need buffer outside the md\n");
		return -1;
	}
	p_file->fd = -1;
	return ret;
}

int pacon_mkdir(struct pacon *pacon, const char *path, mode_t mode)
{
	int ret;
	int dir_mode;
	dir_mode = pacon->df_dir_mode;

	if (pacon->perm_info != NULL)
	{
		ret = check_permission(pacon, path, 0, MKDIR_PC);
		if (ret < 0)
			return -1;
		dir_mode = pacon->perm_info->sp_dir_modes[ret];
	}

	if (PARENT_CHECK == 1)
	{
		ret = check_parent_dir(pacon, path);
		if (ret != 0)
		{
			printf("mkdir: parent dir not existed\n");
			return -1;
		}
	}

	struct pacon_stat p_st;
	char val[PSTAT_SIZE];
	if (SERI_TYPE == 0)
	{
		//struct pacon_stat p_st;
		p_st.flags = 0;
		p_st.mode = dir_mode;
		p_st.ctime = time(NULL);
		p_st.atime = time(NULL);
		p_st.mtime = time(NULL);
		p_st.size = 0;
		p_st.uid = getuid();
		p_st.gid = getgid();
		p_st.nlink = 0;
		p_st.open_counter = 0;
		seri_val(&p_st, val);
evict_ok:
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
	{
		if (ret == -1)
		{
			if (ASYNC_RPC == 1)
			{
				if (pacon->rmdir_record->rmdir_num > 0)
				{
					int ret1 = check_rmdir_list(pacon, path);
					if (ret1 != -1)
					{
						printf("path existed\n");
						return -1;
					} else {
						dmkv_set(pacon->kv_handle, path, val, PSTAT_SIZE);
						goto out;
					}
				}
			}
		}
		if (ret == -2)
		{
			evict_metadata(pacon);
			goto evict_ok;
		}
		return ret;
	}
out:
	//ret = add_to_mq(pacon, path, MKDIR);
	ret = add_to_mq(pacon, path, MKDIR, p_st.ctime);
	return ret;
}

int pacon_getattr(struct pacon *pacon, const char* path, struct pacon_stat* st)
{
	int ret;
	if (pacon->perm_info != NULL)
	{
		ret = check_permission(pacon, path, 1, READ_PC);
		if (ret < 0)
			return -1;
	}

	if (ASYNC_RPC == 1)
	{
		if (pacon->rmdir_record->rmdir_num > 0)
		{
			ret = check_rmdir_list(pacon, path);
			if (ret != 0)
			{
				printf("path is removed\n");
				return -1;
			}
		}
	}

	char *val;
	val = dmkv_get(pacon->kv_handle, path);
	// if not in pacon, try to get from DFS
	if (val == NULL)
	{
		ret = child_cmp_new(path, pacon->mount_path, 1);
		struct stat buf;
		if (ret > 0)
			ret = stat(path, &buf);
		else
			ret = -1;
		if (ret != 0)
		{
			printf("getattr: path not existed: %s\n", path);

			// remote cregion begin
			if (pacon->cr_num > 0)
			{
				int cr;
				for (cr = 0; cr < pacon->cr_num; ++cr)
				{
					ret = child_cmp_new(path, pacon->remote_cr_root[cr], 1);
					if (ret > 0)
					{
						ret = pacon_getattr(pacon->remote_pacon_list[cr], path, st);
						if (ret != 0)
						{
							printf("getattr: path not existed in remote cregions: %s\n", path);
							return -1;
						} else {
							return 0;
						}
					}
				}
				printf("getattr: path not existed in remote cregions: %s\n", path);
				return -1;		
			}
			// remote cregion end

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
evict_ok:
		ret = dmkv_add(pacon->kv_handle, path, val, PSTAT_SIZE);
		if (ret == -2)
		{
			evict_metadata(pacon);
			goto evict_ok;
		}
		goto out;
	}

	// md in pacon, deserialize it
	if (SERI_TYPE == 0)
	{
		deseri_val(st, val);
		if (get_stat_flag(st, STAT_rm) == 1)
		{
			printf("getattr: path not existed: %s\n", path);
			return -1;
		}
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
	if (pacon->perm_info != NULL)
	{
		ret = check_permission(pacon, path, 1, RM_PC);
		if (ret < 0)
			return -1;
	}

	uint64_t cas, cas_temp;
	//ret = dmkv_del(pacon->kv_handle, path);
	struct pacon_stat p_st;
	char *val;
	val = dmkv_get_cas(pacon->kv_handle, path, &cas);
	if (val == NULL)
	{
		ret = load_to_pacon(pacon, path);
		if (ret == -1)
		{
			printf("rm: file is not existed %s\n", path);
			return -1;
		}
		val = dmkv_get_cas(pacon->kv_handle, path, &cas);
	}
	cas_temp = cas;
	deseri_val(&p_st, val);
	set_stat_flag(&p_st, STAT_rm, 1);
	seri_val(&p_st, val);
	ret = dmkv_cas(pacon->kv_handle, path, val, PSTAT_SIZE, cas_temp);
	while (ret == 1)
	{
		val = dmkv_get_cas(pacon->kv_handle, path, &cas);
		cas_temp = cas;
		deseri_val(&p_st, val);
		set_stat_flag(&p_st, STAT_rm, 1);
		seri_val(&p_st, val);
		ret = dmkv_cas(pacon->kv_handle, path, val, PSTAT_SIZE, cas_temp);
	}
	ret = add_to_mq(pacon, path, RM, time(NULL));
	return ret;
}

/*
 * 1. push rmdir mesg to local rpc queue
 * 2. del the dir item in dmkv when pacon server complete the rmdir process
 * 3. clean parent dir check table
 */
int pacon_rmdir(struct pacon *pacon, const char *path)
{
	int ret;
	if (pacon->perm_info != NULL)
	{
		ret = check_permission(pacon, path, 0, RMDIR_PC);
		if (ret < 0)
			return -1;
	}

	if (ASYNC_RPC == 1)
	{
		if (pacon->rmdir_record->rmdir_num > 0)
		{
			ret = check_rmdir_list(pacon, path);
			if (ret != 0)
			{
				printf("path is removed\n");
				return -1;
			}
		}
	}

	/* old version
	uint64_t cas, cas_temp;
	struct pacon_stat p_st;
	char *val;
	val = dmkv_get_cas(pacon->kv_handle, path, &cas);
	if (val == NULL)
	{
		printf("rmdir: dir is not existed %s\n", path);
		return -1;
	}
	cas_temp = cas;
	deseri_val(&p_st, val);
	set_stat_flag(&p_st, STAT_rm, 1);
	seri_val(&p_st, val);
	ret = dmkv_cas(pacon->kv_handle, path, val, PSTAT_SIZE, cas_temp);
	while (ret == 1)
	{
		val = dmkv_get_cas(pacon->kv_handle, path, &cas);
		cas_temp = cas;
		deseri_val(&p_st, val);
		set_stat_flag(&p_st, STAT_rm, 1);
		seri_val(&p_st, val);
		ret = dmkv_cas(pacon->kv_handle, path, val, PSTAT_SIZE, cas_temp);
	}
	*/

	char *val;
	val = dmkv_get(pacon->kv_handle, path);
	// if not in pacon, try to get from DFS
	if (val == NULL)
	{
		ret = load_to_pacon(pacon, path);
		if (ret == -1)
		{
			printf("rmdir: dir is not existed %s\n", path);
			return -1;
		}
		val = dmkv_get(pacon->kv_handle, path);
	}

	add_to_mq(pacon, path, RMDIR, time(NULL));
	ret = add_to_local_rpc(pacon, path, RMDIR, time(NULL));
	if (ret == -1)
	{
		printf("rmdir error: %s\n", path);
		return -1;
	}

	/* new version */
	ret = dmkv_del(pacon->kv_handle, path);
	if (ret != 0)
	{
		printf("rmdir error: %s\n", path);
		return -1;
	}

	// clean parent dir check
	if (ASYNC_RPC == 0)
		fdht_clean();

	return ret;
}


int pacon_rmdir_clean(struct pacon *pacon)
{
	fdht_clean();
	return 0;
}

/* success: return read bytes, error: return -1 */
int pacon_read(struct pacon *pacon, char *path, struct pacon_file *p_file, char *buf, size_t size, off_t offset)
{
	int ret;
	if (pacon->perm_info != NULL)
	{
		ret = check_permission(pacon, path, 1, READ_PC);
		if (ret < 0)
			return -1;
	}
	/*if (p_file->size == 0)
	{
		buf = NULL;
		return 0;
	}*/

	// remote cregion begin
	if (p_file->hit_remote_cr > 0)
	{
		ret = pacon_read(pacon->remote_pacon_list[p_file->hit_remote_cr], path, p_file, buf, size, offset);
		if (ret == -1)
		{
			printf("read from remote cregion error\n");
			return -1;
		}
		return ret;
	}
	// remote cregion end

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
	if (pacon->perm_info != NULL)
	{
		ret = check_permission(pacon, path, 1, WRITE_PC);
		if (ret < 0)
			return -1;
	}

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

int pacon_fsync(struct pacon *pacon, char *path, struct pacon_file *p_file)
{
	int ret;
	if (pacon->perm_info != NULL)
	{
		ret = check_permission(pacon, path, 1, WRITE_PC);
		if (ret < 0)
			return -1;
	}

	// large file, call DFS fsync
	if (p_file->fd != -1)
	{
		ret = fsync(p_file->fd);
		if (ret == -1)
		{
			printf("call DFS fsync error\n");
			return -1;
		} else {
			return 0;
		}
	}

	// small file, sync inline data to log
	struct pacon_stat st;
	char *val;
	uint64_t cas, cas_temp;
	val = dmkv_get_cas(pacon->kv_handle, path, &cas);
	cas_temp = cas;
	if (val == NULL)
	{
		printf("read: get inline data error\n");
		return -1;
	}
	char inline_data[INLINE_MAX];
	deseri_inline_data(&st, inline_data, val);
	
	// if file already be created in DFS, directly call the fsycn
	if (get_stat_flag(&st, STAT_file_created) == 1 &&
		get_stat_flag(&st, STAT_inline) == 0 &&
		st.size > 0)
	{
		int fd = open(path, 0, 0);
		ret = fsync(fd);
		close(fd);
	} else {
		char buf[PATH_MAX + INLINE_MAX];
		strcpy(buf, path);
		strcpy(buf + strlen(path), inline_data);
		posix_memalign(&inline_data, SECTOR_SIZE, strlen(buf));
		int offset = pacon->log_size;
		ret = write(pacon->fsync_log_fd, inline_data, strlen(buf));
		if (ret <= 0)
		{
			printf("fail to sync file to the log: %s\n", path);
			return -1;
		}
		pacon->log_size = pacon->log_size + ret;

		char tmp_path[PATH_MAX];
		sprintf(tmp_path, "%s%s%s%s%d%s%d", path, "|", pacon->fsync_logfile_path, "|", offset, "|", ret);
		ret = add_to_mq(pacon, tmp_path, FSYNC, st.ctime);
		if (ret != 0)
		{
			printf("fsync add to mq error%s\n", path);
			return -1;
		}
	}
	return 0;
}

void pacon_init_perm_info(struct permission_info *perm_info)
{
	int i;
	for (i = 0; i < SP_LIST_MAX; ++i)
	{
		perm_info->sp_dir_modes[i] = 999;
		perm_info->sp_f_modes[i] = 999;
	}
}

int pacon_set_permission(struct pacon *pacon, struct permission_info *perm_info)
{
	int ret;
	pacon->perm_info = perm_info;
	char *nom_dir_mode = "nom_dir_mode";
	char *nom_f_mode = "nom_f_mode";
	char *sp_path = "sp_path";

	char nom_dir_mode_val[5];
	char nom_f_mode_val[5];
	char sp_val[SP_LIST_MAX*(PATH_MAX+5)];

	sprintf(nom_dir_mode_val, "%d", perm_info->nom_dir_mode);
	sprintf(nom_f_mode_val, "%d", perm_info->nom_f_mode);

	// sp_val: PATH : MODE TYPE | PATH : MODE TYPE | ....
	int i;
	for (i = 0; i < perm_info->sp_num; ++i)
	{
		if (i != (perm_info->sp_num)-1)
		{
			sprintf(sp_val, "%s%s", perm_info->sp_path[i], ":");
			if (perm_info->sp_dir_modes[i] != 999)
			{
				sprintf(sp_val, "%d%s", perm_info->sp_dir_modes[i], "d|");
			} else if (perm_info->sp_f_modes[i] != 999) {
				sprintf(sp_val, "%d%s", perm_info->sp_f_modes[i], "f|");
			} else {
				printf("seri sp val: mask error\n");
				return -1;
			}
		} else {
			sprintf(sp_val, "%s%s", perm_info->sp_path[i], ":");
			if (perm_info->sp_dir_modes[i] != 999)
			{
				sprintf(sp_val, "%d%s%c", perm_info->sp_dir_modes[i], "d", '\0');
			} else if (perm_info->sp_f_modes[i] != 999) {
				sprintf(sp_val, "%d%s%c", perm_info->sp_f_modes[i], "f", '\0');
			} else {
				printf("seri sp val: mask error\n");
				return -1;
			}
		}
	}

	ret = dmkv_add(pacon->kv_handle, nom_dir_mode, nom_dir_mode_val, strlen(nom_dir_mode_val));
	ret = dmkv_add(pacon->kv_handle, nom_f_mode, nom_f_mode_val, strlen(nom_f_mode_val));
	ret = dmkv_add(pacon->kv_handle, sp_path, sp_val, strlen(sp_val));

	return 0;
}

DIR * pacon_opendir(struct pacon *pacon, const char *path)
{
	int ret;
	if (pacon->perm_info != NULL)
	{
		ret = check_permission(pacon, path, 0, READDIR_PC);
		if (ret < 0)
			return -1;
	}

	if (ASYNC_RPC == 1)
	{
		if (pacon->rmdir_record->rmdir_num > 0)
		{
			ret = check_rmdir_list(pacon, path);
			if (ret != 0)
			{
				printf("path is removed\n");
				return -1;
			}
		}
	}

	char *val;
	val = dmkv_get(pacon->kv_handle, path);
	// if not in pacon, try to get from DFS
	if (val == NULL)
	{
		ret = load_to_pacon(pacon, path);
		if (ret == -1)
		{
			printf("readdir: dir is not existed %s\n", path);
			return -1;
		}
		val = dmkv_get(pacon->kv_handle, path);
	}
	
	add_to_mq(pacon, path, READDIR, time(NULL));
	ret = add_to_local_rpc(pacon, path, READDIR, time(NULL));
	if (ret == -1)
	{
		printf("readdir server error: %s\n", path);
		return -1;
	}

	DIR *dir;
	dir = opendir(path);
	return dir;
}


struct dirent * pacon_readdir(DIR *dir)
{
	return readdir(dir);
}

void pacon_closedir(struct pacon *pacon, DIR *dir)
{
	closedir(dir);
}


int pacon_rename(struct pacon *pacon, const char *path, const char *newpath)
{
	int ret;
	if (pacon->perm_info != NULL)
	{
		ret = check_permission(pacon, path, 0, RENAME_PC);
		if (ret < 0)
			return -1;
	}

	if (ASYNC_RPC == 1)
	{
		if (pacon->rmdir_record->rmdir_num > 0)
		{
			ret = check_rmdir_list(pacon, path);
			if (ret != 0)
			{
				printf("path is removed\n");
				return -1;
			}
		}
	}

	char *val;
	// check old file/dir
	val = dmkv_get(pacon->kv_handle, path);
	// if not in pacon, try to get from DFS
	if (val == NULL)
	{
		ret = load_to_pacon(pacon, path);
		if (ret == -1)
		{
			printf("readdir: dir is not existed %s\n", path);
			return -1;
		}
		val = dmkv_get(pacon->kv_handle, path);
	}

	// check new file/dir
	val = dmkv_get(pacon->kv_handle, newpath);
	if (val != NULL)
	{
		if (ASYNC_RPC == 1)
		{
			if (pacon->rmdir_record->rmdir_num > 0)
			{
				ret = check_rmdir_list(pacon, path);
				if (ret == 0)
				{
					printf("rename: new name existed%s\n", newpath);
					return -1;
				}
			}
		}
		printf("rename: new name existed%s\n", newpath);
		return -1;
	}
	ret = load_to_pacon(pacon, newpath);
	if (ret != -1)
	{
		printf("rename: new name existed %s\n", newpath);
		return -1;
	}

	if (strlen(path) > PATH_MAX/2 || strlen(newpath) > PATH_MAX/2)
	{
		printf("rename: path too long\n");
		return -1;
	}
	char joint_path[PATH_MAX];
	sprintf(joint_path, "%s%c%s", path, '-', newpath);
	add_to_mq(pacon, joint_path, RENAME, time(NULL));
	ret = add_to_local_rpc(pacon, joint_path, RENAME, time(NULL));
	if (ret == -1)
	{
		printf("readdir server error: %s\n", path);
		return -1;
	}
	return 0;
}

/*
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


/**************** cregion interfaces ***************/

int init_remote_pacon(struct pacon *pacon, int remote_cr_num)
{
	int ret;
	struct dmkv *kv = (struct dmkv *)malloc(sizeof(struct dmkv));
	ret = dmkv_remote_init(kv, remote_cr_num);
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
    
    // init the root dir of the consistent area
    // get the root dir stat from DFS
    ret = load_to_pacon(pacon, pacon->mount_path);
    if (ret != 0)
    {
    	printf("inital root dir fail\n");
    	return -1;
    }
    add_to_dir_check_table(pacon->mount_path);

    // init permission info
    pacon->perm_info = NULL;
    pacon->df_dir_mode =  S_IFDIR | 0755;
    pacon->df_f_mode = S_IFREG | 0644;
    // init joint cr info
    pacon->cr_num = 0;

	return 0;
}

/*
 * 1. reslove the joint config file 
 * 2. connect to the distributed caches of other cregions
 * 3. get the permisstion infos if other cregions 
 */
int cregion_joint(struct pacon *pacon, int remote_cr_num)
{
	int ret;
	int i;
	if (remote_cr_num < 1 || remote_cr_num > CR_JOINT_MAX)
	{
		printf("cregion num error\n");
		return -1;
	}
	pacon->cr_num = remote_cr_num;

	FILE *fp;
	fp = fopen(CRJ_INFO_PATH, "r");
	if (fp == NULL)
	{
		printf("cannot open config file\n");
		return -1;
	}

	int n = 0;
	char t_path[MOUNT_PATH_MAX];
	while ( fgets(t_path, MOUNT_PATH_MAX, fp) )
	{
		int c;
		for (c = 0; c < MOUNT_PATH_MAX; ++c)
		{
			if (t_path[c] == '\0' || t_path[c] == '\n')
				break;
			pacon->remote_cr_root[n][c] = t_path[c];
		}
		pacon->remote_cr_root[n][c] = '\0';
		n++;
	}
	fclose(fp);

	for (i = 0; i < remote_cr_num; ++i)
	{
		struct pacon *r_pacon = (struct pacon *)malloc(sizeof(struct pacon));
		ret = set_root_path(r_pacon, pacon->remote_cr_root[i]);
		ret = init_remote_pacon(r_pacon, i);
		if (ret != 0)
		{
			printf("init remote pacon error\n");
			return -1;
		}
		pacon->remote_pacon_list[i] = r_pacon;
	}
	return 0;
}

int cregion_split(struct pacon *pacon, int remote_cr_num)
{
	int ret;
	int i;
	for (i = 0; i < remote_cr_num; ++i)
	{
		/*ret = dmkv_free(pacon->remote_pacon_list[i]->kv_handle);
		if (ret != 0)
		{
			printf("free remote pacon error\n");
			return -1;
		}*/
	}
	pacon->cr_num = 0;
	return 0;
}

/*
 * app can impelement a period thread to execute the checkpoint
 * don't call this func in multiple clients
 */ 
int cregion_checkpoint(struct pacon *pacon)
{
	int ret;
	add_to_mq(pacon, pacon->mount_path, CHECKPOINT, time(NULL));
	ret = add_to_local_rpc(pacon, pacon->mount_path, CHECKPOINT, time(NULL));
	if (ret == -1)
	{
		printf("checkpoint error: %s\n", pacon->mount_path);
		return -1;
	}
	return 0;
}

/*
 * Pacon does't monitor the heath status of the modes
 * app can call this func to roll the workspace to the lastest version if some node crash during app run
 */
int cregion_recover(struct pacon *pacon)
{
	char cp_path[PATH_MAX];
	int i;
	for (i = strlen(pacon->mount_path)-1; i >= 0; --i)
	{
		if (pacon->mount_path[i] == '/')
			break;
	}
	sprintf(cp_path, "%s%s%s", CHECKPOINT_PATH, (pacon->mount_path)+i, "/*");

	char rm_cmd[128];
	char rm_head = "rm -rf ";
	sprintf(rm_cmd, "%s%s%s", rm_head, pacon->mount_path, "/*");
	system(rm_cmd);

	char cp_cmd[128];
	char cp_head = "cp -rf ";
	sprintf(cp_cmd, "%s%s%s%s", cp_head, cp_path, " ", pacon->mount_path);
	system(cp_cmd);
	return 0;
}
