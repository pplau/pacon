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
	new_pcache->redis = redisClusterConnect(node_list, HIRCLUSTER_FLAG_ROUTE_USE_SLOTS);
	if (new_pcache->redis == NULL || new_pcache->redis->err)
	{
		printf("can not connect to redis\n");
		return -1;
	}

	// inital temporary local namespace
   	struct local_namespace *loc_ns;
	loc_ns = (struct local_namespace *)malloc(sizeof(struct local_namespace));
	loc_ns->entry_count = 0;
	loc_ns->head = NULL;
	loc_ns->tail = NULL;
	pthread_rwlock_init(&(loc_ns->rwlock), NULL);
	new_pcache->loc_ns = loc_ns;

	// inital commit control info
   	struct commit_ctl *commit_ctl;
	commit_ctl = (struct commit_ctl *)malloc(sizeof(struct commit_ctl));
	commit_ctl->uncommit_count = 0;
	new_pcache->commit_ctl = commit_ctl;

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
	return 0;
}

int pcache_free(struct pcache *pcache)
{
	/* disconnects redis */
	redisClusterFree(pcache->redis);
	free(pcache->loc_ns);
	free(pcache->commit_ctl);
	free(pcache);
	return 0;
}

redisReply* pcache_set(struct pcache *pcache, char *key, struct metadata *md)
{
	cJSON *j_body;
	j_body = cJSON_CreateObject();
	cJSON_AddNumberToObject(j_body, "id", md->id);
	cJSON_AddNumberToObject(j_body, "flags", md->flags);
	cJSON_AddNumberToObject(j_body, "mode", md->mode);
	cJSON_AddNumberToObject(j_body, "ctime", md->ctime);
	cJSON_AddNumberToObject(j_body, "atime", md->atime);
	cJSON_AddNumberToObject(j_body, "mtime", md->mtime);
	cJSON_AddNumberToObject(j_body, "size", md->size);
	cJSON_AddNumberToObject(j_body, "uid", md->uid);
	cJSON_AddNumberToObject(j_body, "gid", md->gid);
	cJSON_AddNumberToObject(j_body, "nlink", md->nlink);
	cJSON_AddNumberToObject(j_body, "fd", md->fd);
	cJSON_AddNumberToObject(j_body, "opt", md->opt);
	char *value = cJSON_Print(j_body);
	return redisClusterCommand(pcache->redis,"SETNX %s %s", key, value);
}

redisReply* pcache_update(struct pcache *pcache, char *key, struct metadata *md)
{
	cJSON *j_body;
	j_body = cJSON_CreateObject();
	cJSON_AddNumberToObject(j_body, "id", md->id);
	cJSON_AddNumberToObject(j_body, "flags", md->flags);
	cJSON_AddNumberToObject(j_body, "mode", md->mode);
	cJSON_AddNumberToObject(j_body, "ctime", md->ctime);
	cJSON_AddNumberToObject(j_body, "atime", md->atime);
	cJSON_AddNumberToObject(j_body, "mtime", md->mtime);
	cJSON_AddNumberToObject(j_body, "size", md->size);
	cJSON_AddNumberToObject(j_body, "uid", md->uid);
	cJSON_AddNumberToObject(j_body, "gid", md->gid);
	cJSON_AddNumberToObject(j_body, "nlink", md->nlink);
	cJSON_AddNumberToObject(j_body, "fd", md->fd);
	cJSON_AddNumberToObject(j_body, "opt", md->opt);
	char *value = cJSON_Print(j_body);
	return redisClusterCommand(pcache->redis,"SET %s %s", key, value);
}

int pcache_get(struct pcache *pcache, char *key, struct metadata *md)
{
	redisReply *reply;
	reply = redisClusterCommand(pcache->redis,"GET %s", key);
	if (reply->integer == 0)
		return 0;

	cJSON *j_body, *j_id, *j_flags, *j_mode, *j_ctime, *j_atime, *j_mtime, *j_size, *j_uid, *j_gid, *j_nlink, *j_fd, *j_opt;
	j_body = cJSON_Parse(reply);
	j_id = cJSON_GetObjectItem(j_body, "id");
	j_flags = cJSON_GetObjectItem(j_body, "flags");
	j_mode = cJSON_GetObjectItem(j_body, "mode");
	j_ctime = cJSON_GetObjectItem(j_body, "ctime");
	j_atime = cJSON_GetObjectItem(j_body, "atime");
	j_mtime = cJSON_GetObjectItem(j_body, "mtime");
	j_size = cJSON_GetObjectItem(j_body, "size");
	j_uid = cJSON_GetObjectItem(j_body, "uid");
	j_gid = cJSON_GetObjectItem(j_body, "gid");
	j_nlink = cJSON_GetObjectItem(j_body, "nlink");
	j_fd = cJSON_GetObjectItem(j_body, "fd");
	j_opt = cJSON_GetObjectItem(j_body, "opt");

	md->id = j_id;
	md->flags = j_flags;
	md->mode = j_mode;
	md->ctime = j_ctime;
	md->atime = j_atime;
	md->mtime = j_mtime;
	md->size = j_size;
	md->uid = j_uid;
	md->gid = j_gid;
	md->nlink = j_nlink;
	md->fd = j_fd;
	md->opt = j_opt;
	return reply->len;
}

redisReply* pcache_del(struct pcache *pcache, char *key)
{
	return redisClusterCommand(pcache->redis,"DEL %s", key);
}
