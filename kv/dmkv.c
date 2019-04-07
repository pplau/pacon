/*
 *  written by Yubo
 */

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <pthread.h>


static char node_address[MAX_NODES] = {
	"10.182.171.1",
	"10.182.171.2"
}


/************* memc3 wrapper **************/

static memcached_st *memc_new(void)
{
	int i, node_num = 
	char config_string[1024];
	memcached_st *memc = NULL;
	memcached_return rc;
	memcached_server_st *servers;
	memc = memcached_create(NULL);
	memcached_behavior_set(memc,MEMCACHED_BEHAVIOR_DISTRIBUTION,MEMCACHED_DISTRIBUTION_CONSISTENT);
	servers = memcached_server_list_append(NULL, "localhost", 11211, &rc);
	for (i = 1; i < node_num; ++i)
	{
		servers = memcached_server_list_append(NULL, "localhost", 11211, &rc);
	}

	rc = memcached_server_push(memc, servers);
	memcached_server_free(servers);
	return memc;
}

static int memc_put(memcached_st *memc, char *key, char *val) 
{
	memcached_return_t rc;
	rc = memcached_set(memc, key, key_len, val, val_len, (time_t) 0, (uint32_t) 0);
	if (rc != MEMCACHED_SUCCESS) {
	return 1;
	}
	return 0;
}

static char* memc_get(memcached_st *memc, char *key) 
{
	memcached_return_t rc;
	char *val;
	size_t len;
	uint32_t flag;
	val = memcached_get(memc, key, key_len, &len, &flag, &rc);
	if (rc != MEMCACHED_SUCCESS) {
	return NULL;
	}
	return val;
}



/************* cluster interfaces ***************/

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

/* reture a target node id */
int dht(struct cluster_info *c_info, unsigned long hash)
{
	return hash % c_info->node_num;
}

int dmkv_init(struct dmkv *dmkv)
{
	memcached_st *memc;
	memc = memc_new();
	dmkv->memc;
}

int dmkv_set(struct dmkv *dmkv, int node, char *key, char *value)
{
	unsigned long hash = crc32(key, strlen(key));
	int node_num = dht(dmkv->c_info, hash);

}

int dmkv_get(struct dmkv *dmkv, int node, char *key, char *value)
{
	unsigned long hash = crc32(key, strlen(key));
}

int dmkv_del(struct dmkv *dmkv, int node, char *key)
{
	unsigned long hash = crc32(key, strlen(key));
}

