/*
 *  Written by Yubo
 */
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include "launch.h"

#define PAREA_MAX_NODE 10
#define PAREA_MAX 1000
#define PFS_MOUNTPOINT "/mnt/beegfs"


struct p_area_detail
{
	char *area_path;
	int node_list[PAREA_MAX_NODE];
};

struct sysstat
{
	int p_area_number;
	struct p_area_detail p_area_detail[PAREA_MAX];
};

struct sysconfig
{

};



int resolve_config(struct sysconfig *conf)
{

}

int main(int argc, char * argv[])
{
	int ret;
	int i;
	struct sysstat *sysstat = (struct sysstat *) malloc(sizeof(struct sysstat));
	ret = resolve_config(&sysstat);
	if (ret != 0)
	{
		printf("resolve config fail\n");
		return -1;
	}	

	for (i = 0; i < sysstat->p_area_number; ++i)
	{
		ret = launch_pcache(argc, argv);
		if (ret != 0)
		{
			printf("launch pare error\n");
			return -1;
		}
	}

	return ret;
}