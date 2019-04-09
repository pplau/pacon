/*
 *  written by Yubo
 */

#include <pthread.h>
#include <libmemcached/memcached.h>

#ifndef DMKV_H
#define DMKV_H

#define MAX_NODES 64



struct cluster_info
{
	int node_num;
	int node_list[MAX_NODES];
};

struct dmkv
{
	struct cluster_info *c_info;
	memcached_st *memc;
	pthread_rwlock_t rwlock_t;
};

int dmkv_init(struct dmkv *dmkv);

int dmkv_free(struct dmkv *dmkv);

int dmkv_set(struct dmkv *dmkv, char *key, char *value);

char* dmkv_get(struct dmkv *dmkv, char *key);

int dmkv_del(struct dmkv *dmkv, char *key);



#endif
