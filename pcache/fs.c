/*
 *  written by Yubo
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include "fs.h"
#include "pcache.h"

#define MD_SIZE 52
#define SIMPLE_MD_SIZE 4


/*********************** tools *************************/

void set_md_flag(struct metadata *md, int flag_type, int val)
{
	if (val == 0)
	{
		// set 0
		md->flags &= ~(1UL << flag_type);
	} else {
		// set 1
		md->flags |= 1UL << flag_type;
	}
}

int get_md_flag(struct metadata *md, int flag_type)
{
	if (((md->flags >> flag_type) & 1) == 1 )
	{
		return 1;
	} else {
		return 0;
	}
}

void set_opt_flag(struct metadata *md, int flag_type, int val)
{
	if (val == 0)
	{
		// set 0
		md->flags &= ~(1UL << flag_type);
	} else {
		// set 1
		md->flags |= 1UL << flag_type;
	}
}

int get_opt_flag(struct metadata *md, int flag_type)
{
	if (((md->flags >> flag_type) & 1) == 1 )
	{
		return 1;
	} else {
		return 0;
	}
}

char* md_to_value(struct metadata *md)
{
	char *value;
	value = (char *)malloc(sizeof(struct metadata));
	memcpy(value, md, sizeof(struct metadata));
	return value;
}

char* generate_key(char *path, char opt_type)
{

}

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


/********************* inner functions ***********************/
/*
 *	1. check the path in the cache
 * 	2. if fail, lookup in MDS
 */
int lookup(struct pcache *pcache, const char *path, struct metadata *md)
{
	int ret;
	redisReply *reply;
	reply = pcache_get(pcache, path);
	if (reply->len > SIMPLE_MD_SIZE)
	{
		struct stat buf;
		if ( stat(path, &buf) != 0 )
			return -1;
		md->id = 0;
		md->flags = 0;
		md->mode = buf.st_mode;
		md->ctime = buf.st_ctime;
		md->atime = buf.st_atime;
		md->mtime = buf.st_mtime;
		md->size = buf.st_size;
		md->uid = buf.st_uid;
		md->gid = buf.st_gid;
		md->nlink = 0;
		pcache_set(pcache, path, (char *) md);
		goto out;
	}

	md = (struct metadata *)(reply->str);
out:
	freeReplyObject(reply);
	return 0;
}

int readdir_local()
{

}

int readdir_remote()
{

}

int readdir_merge()
{
	
}


// *********************** fuse interfaces ****************************
void fs_init(struct fs *fs, char * mount_point)
{
	int ret;

   	// find the pare that including this node
   	struct ifaddrs *id = NULL;
   	struct ifaddrs *temp_addr = NULL;
   	char *ipaddr = NULL;
   	getifaddrs(&id);
   	temp_addr = id;
   	while(temp_addr != NULL)
   	{
   		if(strcmp(temp_addr->ifa_name, "eth1"))
   		{
   			ipaddr = temp_addr->ifa_addr;
   		}
   	}

   	struct pcache *new_pcache;
	new_pcache = (struct pcache *)malloc(sizeof(struct pcache));
	new_pcache->hostname = ipaddr;
	new_pcache->port = 6379;
	new_pcache->timeout = 50000;
	new_pcache->mount_point = mount_point;
	ret = pcache_init(new_pcache);
	if (ret != 0)
	{
		printf("fs init error\n");
		pcache_free(new_pcache);
		return -1;
	}

	fs->mount_point = mount_point;
	fs->pcache = new_pcache;
	return 0;
}

int fs_mkdir(struct fs *fs, const char *path, mode_t mode)
{
	int ret;
	struct metadata *md;
	md = (struct metadata *)malloc(sizeof(struct metadata));
	md->id = 0;
	md->flags = 0;
	md->mode = S_IFDIR | 0755;
	md->ctime = time(NULL);
	md->atime = time(NULL);
	md->mtime = time(NULL);
	md->size = 0;
	md->uid = getuid();
	md->gid = getgid();
	md->nlink = 0;
	//strcpy(metadata->key, path);
	set_opt_flag(metadata, OPT_mkdir, 1);

	// put the new metadata into pcache
	redisReply *reply;
	reply = pcache_set(fs->pcache, key, value);
	return 0;
}

/* 
 * 1. read the entries in the local
 * 2. read the entries in other nodes
 * 3. merge the results
 */
int fs_readdir(struct fs *fs, const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fileInfo)
{
	int ret, entry_count;
	ret = readdir_local();
	ret = readdir_remote();
	entry_count = readdir_merge();

	int i;
	for (i = 0; i < entry_count; ++i)
	{
		if (filler(buf, basename(dir_name), NULL, 0) < 0) 
		{
			printf("filler %s error in readdir\n", dir_name);
			return -1;
		}
	}
}

int fs_getattr(struct fs *fs, const char* path, struct stat* st)
{
	int ret;
	struct metadata *md;
	md = (struct metadata *)malloc(sizeof(struct metadata));
	ret = lookup(fs->pcache, path, md);
	if (ret != 0)
	{
		printf("getattar fail\n");
		return -1;
	}
	st->st_nlink = md->nlink;
	st->st_size = md->size;
	st->st_ctime = md->ctime;
	st->st_uid = md->uid;
	st->st_gid = md->gid;
	st->st_atime = md->atime;
	st->st_mtime = md->mtime;
	return ret;
}

int fs_rmdir(struct fs *fs, const char *path)
{
	int ret;
	struct metadata *md;
	md = (struct metadata *)malloc(sizeof(struct metadata));
}

int fs_rename(struct fs *fs, const char *path, const char *newpath)
{

}

int fs_open(struct fs *fs, const char *path, struct fuse_file_info *fileInfo)
{
	int ret;
	struct metadata *md;
	md = (struct metadata *)malloc(sizeof(struct metadata));
	ret = lookup(fs->pcache, path, md);
	if (ret != 0)
	{
		printf("open file fail\n");
		return -1;
	}
	if (get_md_flag(md->flags, MD_type) == 0)
	{
		printf("it is a directory\n");
		return -1;
	}

	if (md->size > SMALL_FILE_SIZE)
	{
		// large file, access DFS directly

		goto out;
	}
	fileInfo->fh = md->fd;

out:
	return ret;
}

int fs_opendir(struct fs *fs, const char *path, struct fuse_file_info *fileInfo)
{

}

int fs_read(struct fs *fs, const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fileInfo)
{

}

int fs_write(struct fs *fs, const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fileInfo)
{

}

int fs_release(struct fs *fs, const char *path, struct fuse_file_info *fileInfo)
{

}

int fs_create(struct fs *fs, const char * path, mode_t mode, struct fuse_file_info * info)
{

}

int fs_releasedir(struct fs *fs, const char * path, struct fuse_file_info * info)
{

}

int fs_destroy(struct fs *fs)
{

}

int fs_utimens(struct fs *fs, const char * path, const struct timespec tv[2])
{

}

int fs_truncate(struct fs *fs, const char * path, off_t length)
{

}

int fs_unlink(struct fs *fs, const char * path)
{

}

int fs_chmod(struct fs *fs, const char * path, mode_t mode)
{

}

int fs_chown(struct fs *fs, const char * path, uid_t owner, gid_t group)
{

}

int fs_access(struct fs *fs, const char * path, int amode)
{

}

int fs_symlink(struct fs *fs, const char * oldpath, const char * newpath)
{

}

int fs_readlink(struct fs *fs, const char * path, char * buf, size_t size)
{

}

int fs_umount(struct fs *fs)
{

}
