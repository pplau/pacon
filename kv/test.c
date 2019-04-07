/*
 *  written by Yubo
 */

#include <malloc.h>
#include "dmkv.h"


int init_dmkv(struct dmkv *kv)
{

}

int free_dmkv(struct dmkv *kv)
{

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
		ret = dmkv_set();
		if (ret < 0)
		{
			printf("set error, key = \n");
			return -1;
		} else {
			printf("set success\n");
		}
	}

	// get test
	printf("/********** get test **********/\n");
	for (i = 0; i < set_num; ++i)
	{
		ret = dmkv_
		if (ret < 0)
		{
			printf("get error, %d\n", i);
			return -1;
		} else {
			printf("get success, val = %s\n", );
		}
	}

	// del test
	printf("/********** del test **********/\n");
	for (i = 0; i < set_num; ++i)
	{
		/* code */
	}
}

int main(int argc, char const *argv[])
{
	int ret;
	struct dmkv *kv = (struct *dmkv)malloc(sizeof(struct dmkv));
	ret = init_dmkv(kv);
	if (ret != 0)
	{
		printf("init error\n");
		goto out;
	}	

	ret = test();

out:
	return 0;
}