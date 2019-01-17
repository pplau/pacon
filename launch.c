/*
 *  Written by Yubo
 */
#define FUSE_USE_VERSION 30
#include <stdio.h>
#include <string.h>
#include <fuse.h>
#include <malloc.h>
#include "cache/fs.h"


static struct fs *fs;

int fuse_open(const char *path, struct fuse_file_info *fileInfo)
{
	return fs_open(fs, path, fileInfo);
}

int fuse_create(const char * path, mode_t mode, struct fuse_file_info * info)
{
    return fs_create(fs, path, mode, info);
}

int fuse_mkdir(const char *path, mode_t mode)
{
	return fs_mkdir(fs, path, mode);
}

int fuse_opendir(const char *path, struct fuse_file_info *fileInfo)
{
    return fs_opendir(fs, path, fileInfo);
}

int fuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fileInfo)
{
	return fs_readdir(fs, path, buf, filler, offset, fileInfo);
}

int fuse_getattr(const char* path, struct stat* st)
{
    return fs_getattr(fs, path, st);
}

int fuse_rmdir(const char *path)
{
	return fs_rmdir(fs, path);
}

int fuse_rename(const char *path, const char *newpath)
{
	return fs_rename(fs, path, newpath);
}

int fuse_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fileInfo)
{
	return fs_read(fs, path, buf, size, offset, fileInfo);
}

int fuse_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fileInfo)
{
	return fs_write(fs, path, buf, size, offset, fileInfo);
}

int fuse_release(const char *path, struct fuse_file_info *fileInfo)
{
    return fs_release(fs, path, fileInfo);
}

int fuse_releasedir(const char *path, struct fuse_file_info *fileInfo)
{
	return fs_releasedir(fs, path, fileInfo);
}

int fuse_utimens(const char * path, const struct timespec tv[2])
{
	return fs_utimens(fs, path, tv);
}

int fuse_truncate(const char * path, off_t offset)
{
	return fs_truncate(fs, path, offset);
}

int fuse_unlink(const char * path)
{
	return fs_unlink(fs, path);
}

int fuse_chmod(const char * path, mode_t mode)
{
	return fs_chmod(fs, path, mode);
}

int fuse_chown(const char * path, uid_t owner, gid_t group)
{
	return fs_chown(fs, path, owner, group);
}

int fuse_access(const char * path, int amode)
{
	return fs_access(fs, path, amode);
}

int fuse_symlink(const char * oldpath, const char * newpath)
{
	return fs_symlink(fs, oldpath, newpath);
}

int fuse_readlink(const char * path, char * buf, size_t size)
{
	return fs_readlink(fs, path, buf, size);
}

static struct fuse_operations fuse_ops =
{
    .open = fuse_open,
    .mkdir = fuse_mkdir,
    .opendir = fuse_opendir,
    .readdir = fuse_readdir,
    .getattr = fuse_getattr,
    .rmdir = fuse_rmdir,
    .rename = fuse_rename,
    .create = fuse_create,
    .read = fuse_read,
    .write = fuse_write,
    .release = fuse_release,
    .releasedir = fuse_releasedir,
    .utimens = fuse_utimens,
    .truncate = fuse_truncate,
    .unlink = fuse_unlink,
    .chmod = fuse_chmod,
    .chown = fuse_chown,
    .access = fuse_access,
    .symlink = fuse_symlink,
    .readlink = fuse_readlink,
};

static void usage(void)
{
    printf(
    "usage:\n"
    );
}

int launch_pcache(int argc, char * argv[])
{
	int ret;
	int i;

	for (i = 0; i < argc; i++) {
		if (strcmp(argv[i], "--help") == 0) {
			usage();
			return 0;
		}
	}

	int fuse_argc = 0;
	char * fuse_argv[argc];
	for (i = 0; i < argc; i++) {
		if (argv[i] != NULL) {
			fuse_argv[fuse_argc++] = argv[i];
		}
	}

	fs = (struct fs *)malloc(sizeof(struct fs));
	fs_init(fs, argv[1]);
	printf("starting fuse main...\n");
	ret = fuse_main(fuse_argc, fuse_argv, &fuse_ops, NULL);
	printf("fuse main finished, ret %d\n", ret);
	fs_destroy(fs);
	return ret;
}