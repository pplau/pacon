/*
 *  written by Yubo
 */
#ifndef PCACHE_H
#define PCACHE_H

#include <stdio.h>
#include <hiredis/hiredis.h>


// control struct of lazy committing
struct commit_ctl
{
	
};

struct pcache
{
	/* redis info */
	redisContext *redis;
	redisReply *reply;
	const char *hostname;
	int port;
	struct timeval timeout;

	/* mount info */
	char *mount_point;

	/* commit info */
	struct commit_ctl *commit_ctl;
};

int find_parea(char *ipaddr);

int pcache_init(struct pcache *new_pcache);

int pcache_set(struct pcache *pcache, redisReply *reply, char *key, char *value);

int pcache_get(struct pcache *pcache, redisReply *reply, char *key);

int pcache_del(struct pcache *pcache, redisReply *reply, char *key);


/************* commit phase *****************/

void bg_commit();




#endif