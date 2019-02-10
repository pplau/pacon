/*
 *  written by Yubo
 */
#ifndef PCACHE_H
#define PCACHE_H

#include <stdio.h>
#include <hiredis-vip/hiredis.h>
#include <hiredis-vip/hircluster.h>


// control struct of lazy committing
struct commit_ctl
{
	int uncommit_count;
	
};

// used to accelerate readdir
struct local_namespace
{
	int entry_count;

};

struct pcache
{
	/* redis info */
	redisClusterContext *redis;
	//redisReply *reply;
	const char *hostname;
	int port;
	struct timeval timeout;

	/* mount info */
	char *mount_point;

	/* local namespace */
	struct local_namespace *loc_ns;

	/* commit info */
	struct commit_ctl *commit_ctl;
};

int find_parea(char *ipaddr);

int pcache_init(struct pcache *new_pcache);

redisReply* pcache_set(struct pcache *pcache, char *key, char *value);

redisReply* pcache_get(struct pcache *pcache, char *key);

redisReply* pcache_del(struct pcache *pcache, char *key);

int pcache_free(struct pcache *pcache);


/************* commit phase *****************/

void bg_commit();




#endif