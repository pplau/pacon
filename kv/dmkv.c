/*
 *  written by Yubo
 */

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include "dmkv.h"

#define NODE_NUM 1

static char node_address[12][4] = {
	"10.182.171.1",
	"10.182.171.2",
	"10.182.171.3",
	"10.182.171.4",
};

static char test_node_address_1 = "10.182.171.2";

/************* memc3 wrapper **************/

int memc_new(struct dmkv *dmkv)
{
	int i, node_num = dmkv->c_info->node_num;
	char config_string[1024];
	memcached_return rc;
	memcached_server_st *servers;
	dmkv->memc = memcached_create(NULL);
	if (dmkv->memc == NULL)
	{
		printf("memc is NULL\n");
		return -1;
	}
	memcached_behavior_set(dmkv->memc,MEMCACHED_BEHAVIOR_DISTRIBUTION,MEMCACHED_DISTRIBUTION_CONSISTENT);
	//servers = memcached_server_list_append(NULL, test_node_address_1, 11211, &rc);
	for (i = 0; i < node_num; ++i)
	{
		servers = memcached_server_list_append(servers, dmkv->c_info->node_list[i], 11211, &rc);
	}

	rc = memcached_server_push(dmkv->memc, servers);
	memcached_server_free(servers);
	return 0;
}

int memc_put(memcached_st *memc, char *key, char *val) 
{
	memcached_return_t rc;
	size_t key_len = strlen(key);
	size_t val_len = strlen(val);
	rc = memcached_set(memc, key, key_len, val, val_len, (time_t) 0, (uint32_t) 0);
	if (rc != MEMCACHED_SUCCESS)
		return -1;
	return 0;
}

char* memc_get(memcached_st *memc, char *key) 
{
	memcached_return_t rc;
	char *val;
	size_t val_len;
	uint32_t flag = 0;
	size_t key_len = strlen(key);
	val = memcached_get(memc, key, key_len, &val_len, &flag, &rc);
	if (rc != MEMCACHED_SUCCESS)
		return NULL;
	return val;
}

int memc_del(memcached_st *memc, char *key)
{
	memcached_return_t rc;
	size_t key_len = strlen(key);
	time_t expiration = 0;
	rc = memcached_delete(memc, key, key_len, expiration);
	if (rc == MEMCACHED_SUCCESS)
		return 0;
	else 
		return -1;
}


/************* cluster interfaces ***************/

/*
unsigned long crc32(const unsigned char *s, unsigned int len)
{
	unsigned int i;
	unsigned long crc32val;

	crc32val = 0;
	for (i = 0;  i < len;  i ++)
	{
	  crc32val =
	crc32_tab[(crc32val ^ s[i]) & 0xff] ^
	  (crc32val >> 8);
	}
	return crc32val;
}

int dht(struct cluster_info *c_info, unsigned long hash)
{
	return hash % c_info->node_num;
}
*/

int get_cluster_info(struct cluster_info *c_info)
{
	c_info->node_num = NODE_NUM;
	FILE *fp;
	fp = fopen("../config");
	int i = 0;
	while ( !feof(fp) )
	{
		fgets(c_info->node_list[i], 16, fp);
		i++;
		if (i >= MAX_NODES)
		{
			printf("kv cluster overflow\n");
			return -1;
		}
	}
	fclose(fp);
	c_info->node_num = i;
	return 0;
}

int dmkv_init(struct dmkv *dmkv)
{
	int ret;
	pthread_rwlock_init(&(dmkv->rwlock_t), NULL);
	pthread_rwlock_wrlock(&(dmkv->rwlock_t));
	struct cluster_info *c_info = (struct cluster_info *)malloc(sizeof(struct cluster_info));
	get_cluster_info(c_info);
	dmkv->c_info = c_info;
	ret = memc_new(dmkv);
	pthread_rwlock_unlock(&(dmkv->rwlock_t));
	if (ret != 0)
	{
		printf("dmkc init error\n");
		return -1;
	}
	return 0;
}

int dmkv_free(struct dmkv *dmkv)
{
	memcached_free(dmkv->memc);
	free(dmkv->c_info);
	free(dmkv);
	return 0;
}

int dmkv_set(struct dmkv *dmkv, char *key, char *value)
{
	//unsigned long hash = crc32(key, strlen(key));
	//int node_num = dht(dmkv->c_info, hash);
	return memc_put(dmkv->memc, key, value);
}

char* dmkv_get(struct dmkv *dmkv, char *key)
{
	//unsigned long hash = crc32(key, strlen(key));
	return memc_get(dmkv->memc, key);
}

int dmkv_del(struct dmkv *dmkv, char *key)
{
	//unsigned long hash = crc32(key, strlen(key));
	return memc_del(dmkv->memc, key);
}

