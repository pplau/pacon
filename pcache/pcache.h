/*
 *  written by Yubo
 */
#ifndef PCACHE_H
#define PCACHE_H

#include <stdio.h>
#include <hiredis/hiredis.h>

#define NODE_NUM_MAX 128

struct pcarea
{
	int node_num;
	char node_list[NODE_NUM_MAX][15];
};

// 
struct pcache_info
{
	int pcarea_count;
	struct pcarea *pcarea_list_head;
};





#endif