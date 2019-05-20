/*
 *  written by Yubo
 */

#include <malloc.h>
#include "dmkv.h"


int init_dmkv(struct dmkv *kv)
{
	return dmkv_init(kv);
}

int free_dmkv(struct dmkv *kv)
{
	return dmkv_free(kv);
}

int test(struct dmkv *kv)
{
	int set_num = 10;
	int i;
	int ret;
	char *val = "test_val";

	// set test
	printf("/********** set test **********/\n");
	for (i = 0; i < set_num; ++i)
	{
		char set_key[2];
		set_key[0] = i + '0';
		set_key[1] = '\0';
		ret = dmkv_set(kv, set_key, val, strlen(val));
		if (ret < 0)
		{
			printf("set error, key = %s\n", set_key);
			return -1;
		} else {
			printf("set success\n");
		}
	}

	// get test
	printf("/********** get test **********/\n");
	for (i = 0; i < set_num; ++i)
	{
		char get_key[2];
		get_key[0] = i + '0';
		get_key[1] = '\0';
		char *res_val;
		res_val = dmkv_get(kv, get_key);
		if (res_val == NULL)
		{
			printf("get error, %d\n", i);
			return -1;
		} else {
			printf("get success, val = %s\n", res_val);
		}
	}

	// cas test
	printf("/********** cas test **********/\n");
	for (i = 0; i < 2; ++i)
	{
		char get_key[2];
		get_key[0] = '0';
		get_key[1] = '\0';
		char *update = "update"
		char *res_val;
		uint64_t cas;
		if (i == 0)
		{
			res_val = dmkv_get_cas(kv, get_key, &cas);
			if (res_val == NULL)
			{
				printf("get cas error, %d\n", i);
				return -1;
			}
			ret = dmkv_cas(kv, get_key, val, strlen(update), cas);
			if (ret == 1)
			{
				printf("first cas version error\n");
				return -1;
			} else if (ret == 0)
			{
				printf("first cas success\n");
				return -1;
			} else {
				printf("first cas error\n");
				return -1;
			}
		} else {
			ret = dmkv_cas(kv, get_key, val, strlen(update), cas);
			if (ret == 1)
			{
				printf("second cas version error\n");
				return -1;
			} else if (ret == 0)
			{
				printf("second cas success\n");
				return -1;
			} else {
				printf("second cas error\n");
				return -1;
			}
		}
	}

	// del test
	printf("/********** del test **********/\n");
	for (i = 0; i < set_num; ++i)
	{
		char del_key[2];
		del_key[0] = i + '0';
		del_key[1] = '\0';
		ret = dmkv_del(kv, del_key);
		if (ret < 0)
		{
			printf("del error, key = %d\n", i);
			return -1;
		} else {
			printf("del success, key = %s\n", del_key);
		}
	}
	return 0;
}	

int main(int argc, char const *argv[])
{
	int ret;
	struct dmkv *kv = (struct dmkv *)malloc(sizeof(struct dmkv));
	ret = init_dmkv(kv);
	if (ret != 0)
	{
		printf("init error\n");
		goto out;
	}	

	ret = test(kv);
	if (ret != 0)
	{
		printf("test error\n");
		goto out;
	}

	ret = free_dmkv(kv);
	if (ret != 0)
	{
		printf("free dmkv error\n");
		goto out;
	}
out:
	return 0;
}

