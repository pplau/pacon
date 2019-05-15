/*
 *  written by Yubo
 */

#include <stdio.h>
#include <sys/stat.h>
#include "pacon.h"

int test_init(struct pacon *pacon)
{
	return init_pacon(pacon);
}

int test_free(struct pacon *pacon)
{
	return free_pacon(pacon);
}

int test_mkdir(struct pacon *pacon, char *path, mode_t mode)
{
	return pacon_mkdir(pacon, path, mode);
}

int test_create(struct pacon *pacon, char *path, mode_t mode)
{
	return pacon_create(pacon, path, mode);
}

int test_stat(struct pacon *pacon, char *path, struct pacon_stat* st)
{
	return pacon_getattr(pacon, path, st);
}

int test_readdir(struct pacon *pacon, char *path)
{
	return 0;
}

int test_rm(struct pacon *pacon, char *path)
{
	return pacon_rm(pacon, path);
}

/*
int test_open(char *path, int flag, mode_t mode)
{
	return pacon_open(path, flag, mode);
}

int test_rm_dir()
{
	return 0;
}

int test_chmod()
{
	return 0;
}

int exc_test()
{
	return 0;
}
*/

int main(int argc, char const *argv[])
{
	int ret;
	struct pacon *pacon = (struct pacon *)malloc(sizeof(struct pacon));
	/*pacon->mount_path[0] = '/';
	pacon->mount_path[1] = '\0'; */
	ret = set_root_path(pacon, "/mnt/beegfs");
	if (ret != 0)
	{
		printf("init pacon error\n");
		return -1;
	}
	pacon->kv_type = 0;		
	ret = test_init(pacon);
	if (ret != 0)
	{
		printf("init pacon error\n");
		return -1;
	}
	printf("init pacon success\n");

	printf("/********** mkdir test **********/\n");
	ret = test_mkdir(pacon, "/mnt/beegfs/test", S_IFDIR | 0755);
	if (ret != 0)
	{
		printf("mkdir error\n");
		return -1;
	}
	printf("mkdir test success, path: /mnt/beegfs/test\n");
	ret = test_mkdir(pacon, "/mnt/beegfs/test/t1", S_IFDIR | 0755);
	if (ret != 0)
	{
		printf("mkdir error\n");
		return -1;
	}
	printf("mkdir test success, path: /mnt/beegfs/test/t1\n");

	printf("/********** create test **********/\n");
	ret = test_create(pacon, "/mnt/beegfs/file", S_IFREG | 0644);
	if (ret != 0)
	{
		printf("create error\n");
		return -1;
	}
	printf("create file test succee, path: /mnt/beegfs/file\n");

	printf("/********** stat test **********/\n");
	struct pacon_stat* st = (struct pacon_stat *)malloc(sizeof(struct pacon_stat));
	ret = test_stat(pacon, "/mnt/beegfs/file", st);
	if (ret != 0)
	{
		printf("stat error\n");
		return -1;
	} else {
		printf("mode: %d\n", st->mode);
		printf("size: %d\n", st->size);
	}
	printf("stat test succee\n");

	printf("/********** rm test **********/\n");
	ret = test_rm(pacon, "/mnt/beegfs/file");
	if (ret != 0)
	{
		printf("rm error\n");
		return -1;
	}
	ret = test_stat(pacon, "/mnt/beegfs/file", st);
	if (ret != 0)
		printf("rm test succee\n");
	else
		printf("rm test error\n");

	ret = test_free(pacon);
	if (ret != 0)
	{
		printf("free pacon error\n");
		return -1;
	}
	return 0;
}
