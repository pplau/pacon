/*
 *  written by Yubo
 */

#include <stdio.h>
#include "pcache.h"


static char *node_list = "10.182.171.1:6379,10.182.171.2:6379,10.182.171.3:6379,10.182.171.4:6379,\
							10.182.171.5:6379,10.182.171.6:6379,10.182.171.7:6379,10.182.171.8:6379,\
							10.182.171.9:6379,10.182.171.10:6379,10.182.171.11:6379,10.182.171.12:6379,\
							10.182.171.13:6379,10.182.171.14:6379,10.182.171.15:6379,10.182.171.16:6379,\
							10.182.171.17:6379,10.182.171.18:6379,10.182.171.19:6379,10.182.171.20:6379,\
							10.182.171.21:6379,10.182.171.22:6379,10.182.171.23:6379,10.182.171.24:6379,\
							10.182.171.25:6379,10.182.171.26:6379,10.182.171.27:6379,10.182.171.28:6379,\
							10.182.171.29:6379,10.182.171.30:6379,10.182.171.31:6379,10.182.171.32:6379,\
							10.182.172.1:6379,10.182.172.2:6379,10.182.172.3:6379,10.182.172.4:6379,\
							10.182.172.5:6379,10.182.172.6:6379,10.182.172.7:6379,10.182.172.8:6379,\
							10.182.172.9:6379,10.182.172.10:6379,10.182.172.11:6379,10.182.172.12:6379,\
							10.182.172.13:6379,10.182.172.14:6379,10.182.172.15:6379,10.182.172.16:6379,\
							10.182.172.17:6379,10.182.172.18:6379,10.182.172.19:6379,10.182.172.20:6379,\
							10.182.172.21:6379,10.182.172.22:6379,10.182.172.23:6379,10.182.172.24:6379,\
							10.182.172.25:6379,10.182.172.26:6379,10.182.172.27:6379,10.182.172.28:6379,\
							10.182.172.29:6379,10.182.172.30:6379,10.182.172.31:6379,10.182.172.32:6379";


int find_parea(char *ipaddr)
{

}

int pcache_init(struct pcache *new_pcache)
{
	/* connect to redis */
	new_pcache->redis = redisClusterContextInit();
	redisClusterSetOptionAddNodes(new_pcache->redis, node_list);
	redisClusterConnect2(new_pcache->redis);
	//new_pcache->redis = redisConnect(new_pcache->hostname, new_pcache->port);
	if (new_pcache->redis == NULL || new_pcache->redis->err)
	{
		printf("can not connect to redis\n");
		return -1;
	}

	char *key = "foo";
	char *value = "bar";

	redisReply *reply = redisClusterCommand(new_pcache->redis, "set %s %s", key, value);
    if(reply == NULL)
    {
        printf("reply is null[%s]\n", cc->errstr);
        redisClusterFree(cc);
        return -1;
    }
    printf("get: %s\n", reply->str);
    freeReplyObject(reply);

	/* construct namespace 
	char *nsinfo_path;
	FILE *fptr = fopen(new_pcache->mount_point, "r");
	if (fptr == NULL)
		goto out;*/
	// get existing namespace data and put them into the pcache 


out:
	return 0;
}

int pcache_clean(struct pcache *pcache)
{

}

int pcache_free(struct pcache *pcache)
{
	/* disconnects redis */
	redisClusterFree(pcache->redis);
	free(pcache);
	return 0;
}

int pcache_set(struct pcache *pcache, redisReply *reply, char *key, char *value)
{
	reply = redisClusterCommand(pcache->redis,"SETNX %s %s", key, value);
	return 0;
}

int pcache_update(struct pcache *pcache, redisReply *reply, char *key, char *value)
{
	reply = redisClusterCommand(pcache->redis,"SET %s %s", key, value);
	return 0;
}

int pcache_get(struct pcache *pcache, redisReply *reply, char *key)
{
	reply = redisClusterCommand(pcache->redis,"GET %s", key);
	return 0;
}

int pcache_del(struct pcache *pcache, redisReply *reply, char *key)
{
	reply = redisClusterCommand(pcache->redis,"DEL %s", key);
	return 0;
}
