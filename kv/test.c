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
		char key = i + '0';
		ret = dmkv_set(kv, &key, val);
		if (ret < 0)
		{
			printf("set error, key = %s\n", &key);
			return -1;
		} else {
			printf("set success\n");
		}
	}

	// get test
	printf("/********** get test **********/\n");
	for (i = 0; i < set_num; ++i)
	{
		char key = i + '0';
		char *res_val;
		res_val = dmkv_get(kv, &key);
		if (res_val == NULL)
		{
			printf("get error, %d\n", i);
			return -1;
		} else {
			printf("get success, val = %s\n", res_val);
		}
	}

	// del test
	printf("/********** del test **********/\n");
	for (i = 0; i < set_num; ++i)
	{
		char key = i + '0';
		ret = dmkv_del(kv, &key);
		if (ret < 0)
		{
			printf("del error, key = %d\n", i);
			return -1;
		} else {
			printf("del success, key = %s\n", &key);
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

