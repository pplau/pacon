/*
 *  written by Yubo
 */
#ifndef DMKV_H
#define DMKV_H

#include <pthread.h>
#include <libmemcached/memcached.h>
#include "../pacon.h"

#define NODE_MAX 64


struct cluster_info
{
	int node_num;
	char node_list[NODE_MAX][17];
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

int dmkv_add(struct dmkv *dmkv, char *key, char *val);

int dmkv_cas(struct dmkv *dmkv, char *key, char *val);

int dmkv_check(struct dmkv *dmkv, char *key);

char* dmkv_get(struct dmkv *dmkv, char *key);

int dmkv_del(struct dmkv *dmkv, char *key);



#endif
