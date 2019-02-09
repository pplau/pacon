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
	reply = pcache_set(pc, key, value);
	printf("set success\n");
	freeReplyObject(reply);

	reply = pcache_get(pc, key);
	printf("get key: %s\n", reply->str);
	freeReplyObject(reply);

	reply = pcache_update(pc, key, new_value);
	freeReplyObject(reply);
	reply = pcache_get(pc, key);
	printf("get key after update: %s\n", reply->str);
	freeReplyObject(reply);

	reply = pcache_del(pc, key);
	printf("get key after del: %s\n", reply->str);
	freeReplyObject(reply);
	
	ret = pcache_free(pc);
	printf("free success\n");

	return 0;
}