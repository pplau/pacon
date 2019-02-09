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

	int ret;
	redisReply *reply;
	reply = (struct redisReply *)malloc(sizeof(struct redisReply));
	ret = pcache_set(pc, reply, key, value);
	printf("set success\n");
	ret = pcache_get(pc, reply, key);
	printf("get key: %s\n", reply->str);
	ret = pcache_update(pc, reply, key, new_value);
	ret = pcache_get(pc, reply, key);
	printf("get key after update: %s\n", reply->str);
	ret = pcache_del(pc, reply, key);
	printf("get key after del: %s\n", reply->str);
	ret = pcache_free(pc);
	printf("free success\n");

	return 0;
}