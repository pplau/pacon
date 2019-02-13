#include <stdio.h>
#include "pcache.h"



int main(int argc, char const *argv[])
{
	struct pcache *pc;
	pc = (struct pcache *)malloc(sizeof(struct pcache));
	pc->hostname = "10.182.171.2";
	pc->port = 6379;
	//pc->timeout = { 1, 500000 }; // 1.5s
	pcache_init(pc);
	char *key = "test";
	char *value = "fuck you";
	char *new_value ="qqqqq";

	struct metadata *md;
	md = (struct metadata *)malloc(sizeof(struct metadata));
	md->id = 0;
	md->flags = 0;
	md->mode = S_IFDIR | 0755;
	md->ctime = time(NULL);
	md->atime = time(NULL);
	md->mtime = time(NULL);
	md->size = 0;
	md->uid = getuid();
	md->gid = getgid();
	md->nlink = 0;
	md->fd = 0;
	md->opt = 0;

	int ret;
	redisReply *reply;
	reply = pcache_set(pc, key, md);
	printf("set success\n");
	freeReplyObject(reply);

	reply = pcache_get(pc, key);
	printf("get key: %s\n", reply->str);
	freeReplyObject(reply);

	/*reply = pcache_update(pc, key, new_value);
	freeReplyObject(reply);
	reply = pcache_get(pc, key);
	printf("get key after update: %s\n", reply->str);
	freeReplyObject(reply);*/

	reply = pcache_del(pc, key);
	printf("get key after del: %s\n", reply->str);
	freeReplyObject(reply);

	ret = pcache_free(pc);
	printf("free success\n");

	return 0;
}