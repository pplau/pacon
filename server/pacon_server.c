/*
 *  written by Yubo
 */

#include <zmq.h>
#include "pacon_server.h"


int start_pacon()
{

}

int stop_pacon()
{

}

int commit()
{

}


int main(int argc, char const *argv[])
{
	int ret;
	printf("Strat Pacon Server\n");
	ret = start_pacon();
	if (ret != 0)
	{
		printf("strat pacon error\n");
		return -1;
	}

	while (1)
	{

	}

	return 0;
}