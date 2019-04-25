/*
 *  written by Yubo
 */
#ifndef PACON_SERVER_H
#define PACON_SERVER_H

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

struct carea_info
{
	
};

struct pacon_server_info
{
	int carea_num;
	struct carea_info *carea_list;
};


int start_pacon();

int stop_pacon();

int commit();

int new_carea();

int free_carea();

#endif