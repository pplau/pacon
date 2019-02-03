/*
 *  written by Yubo
 */

#include <stdio.h>
#include "pcache.h"


int find_parea(char *ipaddr)
{

}

int pcache_init(struct pcache *new_pcache)
{
	/* connect to redis */
	new_pcache->redis = redisConnectWithTimeout(new_pcache->hostname, new_pcache->port, new_pcache->timeout);
	if (new_pcache.redis == NULL || new_pcache.redis->err)
	{
		printf("can not connect to redis\n");
		return -1;
	}

	/* construct namespace */

	return 0;
}

int pcache_free(struct pcache *pcache)
{
	/* disconnects redis */
	redisFree(pcache->redis);
	return 0;
}

int pcache_set(struct pcache *pcache, redisReply *reply, char *key, char *value)
{
	reply = redisCommand(pcache->redis,"SET %s %s", key, value);
	return 0;
}

int pcache_get(struct pcache *pcache, redisReply *reply, char *key)
{
	reply = redisCommand(pcache->redis,"GET %s", key);
	return 0;
}

int pcache_del(struct pcache *pcache, redisReply *reply, char *key)
{
	reply = redisCommand(pcache->redis,"DEL %s", key);
	return 0;
}