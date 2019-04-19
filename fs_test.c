/*
 *  written by Yubo
 */

#include <stdio.h>
#include <sys/stat.h>
#include "pacon.h"

int init_pacon(struct pacon *pacon)
{
	return init_pacon(pacon);
}

int free_pacon(struct pacon *pacon)
{
	return free_pacon(pacon);
}

int test_mkdir(char *path, mode_t mode)
{
	return pacon_mkdir(path, mode);
}

/*
int test_create()
{
	return 0;
}

int test_readdir()
{
	return 0;
}

int test_open()
{
	return 0;
}

int test_rm()
{
	return 0;
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
	pacon->mount_path[0] = '/';
	pacon->mount_path[1] = '\0';
	pacon->kv_type = 0;
	ret = init_pacon(pacon);
	if (ret != 0)
	{
		printf("init pacon error\n");
		return -1;
	}

	printf("mkdir test\n");
	ret = test_mkdir("/test", S_IFDIR | 0755);
	if (ret != 0)
	{
		printf("mkdir error\n");
		return -1;
	}

	printf("create file test\n");
	ret = test_create("/file", S_IFREG | 0644);
	if (ret != 0)
	{
		printf("create error\n");
		return -1;
	}

	ret = free_pacon(pacon);
	if (ret != 0)
	{
		printf("free pacon error\n");
		return -1;
	}
	return 0;
}
