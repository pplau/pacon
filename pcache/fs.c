/*
 *  written by Yubo
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "fs.h"
#include "pcache.h"


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

}

int fs_readdir(struct fs *fs, const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fileInfo)
{

}

int fs_getattr(struct fs *fs, const char* path, struct stat* st)
{

}

int fs_rmdir(struct fs *fs, const char *path)
{

}

int fs_rename(struct fs *fs, const char *path, const char *newpath)
{

}

int fs_open(struct fs *fs, const char *path, struct fuse_file_info *fileInfo)
{

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
