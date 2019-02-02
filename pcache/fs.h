/*
 *  written by Yubo
 */
#ifndef FS_H
#define FS_H

#define DIR_META 0
#define FILE_META 1


struct metadata
{
	int md_type;  // DIR_META or FILE_META
	uint32_t id;
	uint32_t flags;   //  
	char key[MAX_DENTRY_NAME];  // full path
	uint32_t mode;
	uint32_t ctime;
	uint32_t atime;
	uint32_t mtime;
	uint32_t size;
	uint32_t uid;
	uint32_t gid;
	uint32_t nlink;
};

void md_to_value(struct metadata *md);



// *********************** fuse interfaces ****************************
void fs_init(struct fs *fs, char * mount_point);

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