/*
 *  written by Yubo
 */

#include <stdio.h>
#include <sys/stat.h>
#include <time.h>
#include <dirent.h>
#include <unistd.h>
#include "pacon.h"

#define TEST_TYPE 0  // 0 is mirco test, 1 is pressure test, 2 is cregion joint test
					 // 3 is eviction test, 4 is batch permission test


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

int test_rm(struct pacon *pacon, char *path)
{
	return pacon_rm(pacon, path);
}

int test_open_wr(struct pacon *pacon, char *path, int flag, mode_t mode)
{
	int ret;
	struct pacon_file *p_file = new_pacon_file();
	ret =  pacon_open(pacon, path, flag, mode, p_file);
	if (ret == -1)
	{
		printf("fail to open file\n");
		return -1;
	}
	char *data = "hello, you are a pig";
	int size = strlen(data);
	ret = pacon_write(pacon, path, p_file, data, size, 0);
	if (ret == -1)
	{
		printf("write file error\n");
		return -1;
	}
	char out[128];
	ret = pacon_read(pacon, path, p_file, out, size, 0);
	if (ret <= 0)
	{
		printf("read file error\n");
		return -1;
	}
	printf("file data: %s\n", out);
	//pacon_close(pacon, p_file);

	// test rewrite
	char *update = "Opsss";
	int update_size = strlen(update);
	ret = pacon_write(pacon, path, p_file, update, update_size, 0);
	if (ret == -1)
	{
		printf("rewrite error\n");
		return -1;
	}
	char up_out[128];
	ret = pacon_read(pacon, path, p_file, out, size, 0);
	if (ret <= 0)
	{
		printf("read file error\n");
		return -1;
	}
	printf("file data: %s\n", out);
	return ret;
}

int test_create_write(struct pacon *pacon, char *path, int flag, mode_t mode)
{
	int ret;
	struct pacon_file *p_file = new_pacon_file();
	char *data = "test write after create";
	int size = strlen(data);
	ret = pacon_create_write(pacon, path, mode, data, size, p_file);
	if (ret == 0)
	{
		printf("write after create success\n");
	} else {
		printf("write after create error\n");
		return -1;
	}

	// read it for test
	//struct pacon_file *p_file = new_pacon_file();
	ret =  pacon_open(pacon, path, flag, mode, p_file);
	if (ret == -1)
	{
		printf("fail to open file\n");
		return -1;
	}
	char out[128];
	ret = pacon_read(pacon, path, p_file, out, size, 0);
	if (ret <= 0)
	{
		printf("read file error\n");
		return -1;
	}
	printf("file data: %s\n", out);
	return ret;
}

int test_fsync(struct pacon *pacon, char *path)
{
	int ret;
	struct pacon_file *p_file = new_pacon_file();
	ret =  pacon_open(pacon, path, 0, 0, p_file);
	if (ret == -1)
	{
		printf("fail to open file\n");
		return -1;
	}
	ret = pacon_fsync(pacon, path, p_file);
	if (ret != 0)
	{
		printf("fsync error\n");
		return -1;
	}
	return 0;
}

int test_readdir(struct pacon *pacon, char *path)
{
	int ret;
	DIR *dir;
	struct dirent *entry;
	dir = pacon_opendir(pacon, path);
	entry = pacon_readdir(dir);
	while (entry != NULL)
	{
		printf("entry name: %s\n", entry->d_name);
		entry = pacon_readdir(dir);
	}
	pacon_closedir(pacon, dir);
	return 0;
}

int test_rename(struct pacon *pacon, char *oldpath, char *newpath)
{
	return pacon_rename(pacon, oldpath, newpath);
}

int test_rmdir(struct pacon *pacon, char *path)
{
	return pacon_rmdir(pacon, path);
}

/*
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

void batch_mkdir(struct pacon *pacon, char* p_dir, int num)
{
	int i, ret, p_len = strlen(p_dir);
	char dir[128];
	memcpy(dir, p_dir, p_len);
	dir[p_len] = '/';
	char c[8];
	for (i = 0; i < num; ++i)
	{
		sprintf(c, "%d", i);
		int c_len = strlen(c);
		memcpy(dir+p_len+1, c, c_len);
		dir[p_len+c_len+1] = '\0';
		ret = pacon_mkdir(pacon, dir, S_IFDIR | 0755);
		if (ret != 0)
		{
			printf("batch mkdir error, path: %s\n", dir);
			break;
		}
	}
}

int main(int argc, char const *argv[])
{
	int ret;
	struct pacon *pacon = (struct pacon *)malloc(sizeof(struct pacon));
	/*pacon->mount_path[0] = '/';
	pacon->mount_path[1] = '\0'; */
    if (TEST_TYPE == 0 || TEST_TYPE == 1 || TEST_TYPE == 4)
    {
            ret = set_root_path(pacon, "/mnt/beegfs");
    } else if (TEST_TYPE == 2) {
    	ret = set_root_path(pacon, "/mnt/beegfs/cr0");
    } else if (TEST_TYPE == 3) {
    	ret = set_root_path(pacon, "/mnt/beegfs/pacon");
    }
    
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

	if (TEST_TYPE == 0)
	{
		printf("\n");
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

		printf("\n");
		printf("/********** create test **********/\n");
		ret = test_create(pacon, "/mnt/beegfs/file", S_IFREG | 0644);
		if (ret != 0)
		{
			printf("create error\n");
			return -1;
		}
		printf("create file test succee, path: /mnt/beegfs/file\n");
		ret = test_create(pacon, "/mnt/beegfs/test/f1", S_IFREG | 0644);
		if (ret != 0)
		{
			printf("create error\n");
			return -1;
		}
		printf("create file test succee, path: /mnt/beegfs/test/f1\n");

		printf("\n");
		printf("/********** open/rw test **********/\n");
		ret = test_open_wr(pacon, "/mnt/beegfs/file", 0, 0);
		if (ret <= 0)
		{
			printf("open/rw error\n");
			return -1;
		}
		printf("open/rw success\n");

		printf("\n");
		printf("/********** create_write test **********/\n");
		ret = test_create_write(pacon, "/mnt/beegfs/file_cw", 0, 0);
		if (ret <= 0)
		{
			printf("create_write error\n");
			return -1;
		}
		printf("create_write success\n");

		printf("\n");
		printf("/********** fsync test **********/\n");
		ret = test_fsync(pacon, "/mnt/beegfs/file_cw");
		if (ret != 0)
		{
			printf("fsync error\n");
			return -1;
		}
		printf("fsync success\n");

		printf("\n");
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

		printf("\n");
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

		printf("\n");
		printf("/********** readdir test **********/\n");
		pacon_barrier(pacon);
		ret = test_readdir(pacon, "/mnt/beegfs/test");
		if (ret == 0)
			printf("readdir test succee\n");
		else
			printf("readdir test error\n");

		//printf("\n");
		//printf("/********** rename test **********/\n");
		/*ret = test_rename(pacon, "/mnt/beegfs/test", "/mnt/beegfs/test1");
		if (ret == 0)
			printf("rename test succee\n");
		else
			printf("rename test error\n");*/

		printf("\n");
		printf("/********** rmdir test **********/\n");
		pacon_barrier(pacon);
		ret = test_rmdir(pacon, "/mnt/beegfs/test");
		if (ret != 0)
		{
			printf("rmdir error\n");
			return -1;
		}
		ret = test_stat(pacon, "/mnt/beegfs/test", st);
		if (ret != 0)
			printf("rmdir test succee\n");
		else
			printf("rmdir test error\n");

		printf("\n");
		printf("/********** free pacon **********/\n");
		ret = test_free(pacon);
		if (ret != 0)
		{
			printf("free pacon error\n");
			return -1;
		}
		printf("free pacon success\n");
	}

	if (TEST_TYPE == 1)
	{
		int test_num = 1000;
		struct timespec start;
		struct timespec end;
		clock_gettime(CLOCK_REALTIME, &start);
		batch_mkdir(pacon, "/mnt/beegfs/pacon", test_num);
		clock_gettime(CLOCK_REALTIME, &end);
        long timediff;
        timediff = 1000000 * (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1000;
        printf("batch mkdir %ld us\n", timediff);
        //printf("throughput: %ld OPS\n", test_num/t);
	}

	if (TEST_TYPE == 2)
	{
		int cr_num = 1;
		int num = 0;

		// create dir and file
		if (num == 0)
		{
			ret = test_mkdir(pacon, "/mnt/beegfs/cr0/d", S_IFDIR | 0755);
			//ret = test_create(pacon, "/mnt/beegfs/cr0/f", S_IFREG | 0644);
			if (ret != 0)
			{
				printf("mkdir in cr0 error\n");
				return -1;
			}
		}

		// joint to cregion 0 and stat dir/file in it
		if (num == 1)
		{
			ret = cregion_joint(pacon, cr_num);
			if (ret != 0)
			{
				printf("joint cregions error\n");
				return -1;
			}
			struct pacon_stat* r_st = (struct pacon_stat *)malloc(sizeof(struct pacon_stat));
			ret = test_stat(pacon, "/mnt/beegfs/cr0/d", r_st);
			if (ret == 0)
			{
				printf("mode: %d\n", r_st->mode);
				printf("size: %d\n", r_st->size);
				printf("stat joint cregions success\n");
			} else {
				printf("stat joint cregions error\n");
				return -1;
			}
			ret = cregion_split(pacon, cr_num);
			if (ret == 0)
			{
				printf("split cregions success\n");
			} else {
				printf("split cregions error\n");
				return -1;
			}
		}

		ret = test_free(pacon);
		if (ret != 0)
		{
			printf("free pacon error\n");
			return -1;
		}
		printf("free pacon success\n");
	}

	if (TEST_TYPE == 3)
	{
		int ret1, ret2, ret3;
		ret1 = test_mkdir(pacon, "/mnt/beegfs/pacon/test", S_IFDIR | 0755);
		ret2 = test_mkdir(pacon, "/mnt/beegfs/pacon/test/d1", S_IFDIR | 0755);
		ret3 = test_create(pacon, "/mnt/beegfs/pacon/test/f1", S_IFREG | 0644);
		if (ret1 != 0 || ret2 != 0 || ret3 != 0)
		{
			printf("mkdir or create error\n");
			return -1;
		}
		ret = evict_metadata(pacon);
		if (ret != 0)
		{
			printf("evict error 1\n");
			return -1;
		}
		/*struct pacon_stat* r_st = (struct pacon_stat *)malloc(sizeof(struct pacon_stat));
		ret = test_stat(pacon, "/mnt/beegfs/pacon/test/d1", r_st);
		if (ret != 0)
		{
			printf("evict error 2\n");
			return -1;
		} else {
			printf("evict test success\n");
		}*/
	}

	if (TEST_TYPE == 4)
	{
		struct permission_info *perm_info = (struct permission_info *)malloc(sizeof(struct permission_info));
		pacon_init_perm_info(perm_info);
		perm_info->nom_dir_mode = 755;
		perm_info->nom_f_mode = 644;
		perm_info->sp_num = 1;
		char *sp = "/mnt/beegfs/sp";
		sprintf(perm_info->sp_path[0], "%s", sp);
		perm_info->sp_dir_modes[0] = 111;
		
		ret = pacon_set_permission(pacon, perm_info);
		if (ret != 0)
		{
			printf("batch set permission error\n");
			return -1;
		}
		ret = test_mkdir(pacon, "/mnt/beegfs/sp", S_IFDIR | 0755);
		if (ret != 0)
		{
			printf("sp path mkdir error\n");
			return -1;
		}
		ret = test_mkdir(pacon, "/mnt/beegfs/sp/d1", S_IFDIR | 0755);
		if (ret != 0)
		{
			printf("batch permission test success\n");
		} else {
			printf("batch permission test error\n");
		}
	}

	return 0;
}
