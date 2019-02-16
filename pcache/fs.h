/*
 *  written by Yubo
 */
#ifndef FS_H
#define FS_H

#include <fuse.h>
#include <errno.h>
#include "pcache.h"

#define DIR_META 0
#define FILE_META 1
#define SMALL_FILE_SIZE 4096

// general return state
#define SUCCESS 0
#define ERROR -1

// lookup state
#define COMP_HIT 0
#define SIMP_HIT 1
#define LOOKUP_MISS -1


enum metadataflags
{
	MD_type,  // 0 is directory, 1 is file
	MD_cache, // 1 is cached
	MD_commit, // 0 is committed, 1 is uncommit
	MD_file_open, // for file metadata, 0 is close, 1 is open
};

enum operations
{
	OP_mkdir,  // 
	OP_create,  // 
	OP_chmod, //
	OP_rm, //
	OP_rmdir,
};

struct fs
{
	char *mount_point;
	struct pcache *pcache;
};

struct metadata
{
	uint32_t id;
	uint32_t flags;   //  including the metadata type
	//char key[MAX_DENTRY_NAME];  // full path
	uint32_t mode;
	uint32_t ctime;
	uint32_t atime;
	uint32_t mtime;
	uint32_t size;
	uint32_t uid;
	uint32_t gid;
	uint32_t nlink;
	uint32_t fd;
	uint32_t opt;
};

struct simple_metadata
{
	int flags;
};

char* md_to_value(struct metadata *md);



// *********************** fuse interfaces ****************************
int fs_init(struct fs *fs, char *node_list, char *mount_point);

int fs_mkdir(struct fs *fs, const char *path, mode_t mode);

int fs_readdir(struct fs *fs, const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fileInfo);

int fs_getattr(struct fs *fs, const char* path, struct stat* st);

int fs_rmdir(struct fs *fs, const char *path);

int fs_rename(struct fs *fs, const char *path, const char *newpath);

int fs_open(struct fs *fs, const char *path, struct fuse_file_info *fileInfo);

int fs_opendir(struct fs *fs, const char *path, struct fuse_file_info *fileInfo);

int fs_read(struct fs *fs, const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fileInfo);

int fs_write(struct fs *fs, const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fileInfo);

int fs_release(struct fs *fs, const char *path, struct fuse_file_info *fileInfo);

int fs_create(struct fs *fs, const char * path, mode_t mode, struct fuse_file_info * info);

int fs_releasedir(struct fs *fs, const char * path, struct fuse_file_info * info);

int fs_destroy(struct fs *fs);

int fs_utimens(struct fs *fs, const char * path, const struct timespec tv[2]);

int fs_truncate(struct fs *fs, const char * path, off_t length);

int fs_unlink(struct fs *fs, const char * path);

int fs_chmod(struct fs *fs, const char * path, mode_t mode);

int fs_chown(struct fs *fs, const char * path, uid_t owner, gid_t group);

int fs_access(struct fs *fs, const char * path, int amode);

int fs_symlink(struct fs *fs, const char * oldpath, const char * newpath);

int fs_readlink(struct fs *fs, const char * path, char * buf, size_t size);

int fs_umount(struct fs *fs);

#endif