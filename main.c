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


struct parea_detail
{
	char *area_path;
	char node_list[PAREA_MAX_NODE][15];
};

// cluster stat
struct clstat
{
	int parea_number;
	struct parea_detail parea_detail[PAREA_MAX];
};

/*
struct sysconfig
{

};

int resolve_config(struct sysconfig *conf)
{

}
*/

/*
 * 1. Run redis clusters accroding to the sysconfig
 * 2. Initial each pcache
 * 3. Launch fuse
 */
int main(int argc, char * argv[])
{
	int ret;
	int i;
	struct clstat *clstat = (struct clstat *) malloc(sizeof(struct clstat));
	/*ret = resolve_config(&clstat);
	if (ret != 0)
	{
		printf("resolve config fail\n");
		return -1;
	}*/
	clstat->parea_number = 1;

	ret = launch_pcache(argc, argv, clstat);
	if (ret != 0)
	{
		printf("launch pare error\n");
		return -1;
	}

	return ret;
}