/*
 *  written by Yubo
 */

#include <string>
#include "./libcuckoo/cuckoohash_map.hh"
#include "./hash_table.h"


cuckoohash_map<std::string, int> dir_check_table;

cuckoohash_map<std::string, int> fd_table;

cuckoohash_map<std::string, int> openc_table;


/************* parent dir check hash table *************/

extern "C" int pdirht_put(char *key, int val)
{
	int ret;
	std::string k = key;
	dir_check_table.insert(k, val);
	return 0;
}

extern "C" int pdirht_update(char *key, int val)
{
	return 0;
}

extern "C" int pdirht_get(char *key)
{
	int ret;
	std::string k = key;
	if (dir_check_table.find(k, ret))
	{
		return ret;
	} else {
		//std::cout << "key not existed in dir check table" << std::endl;
		return -1;
	}
}

extern "C" int pdirht_del(char *key)
{
	return 0;
}

extern "C" void pdirht_clean(void)
{
	dir_check_table.clear();
}


/****************** fd hash table ******************/

extern "C" int fdht_put(char *key, int val)
{
	int ret;
	std::string k = key;
	fd_table.insert(k, val);
	return 0;
}

extern "C" int fdht_update(char *key, int val)
{
	return 0;
}

extern "C" int fdht_get(char *key)
{
	int ret;
	std::string k = key;
	if (fd_table.find(k, ret))
	{
		return ret;
	} else {
		//std::cout << "key not existed in dir check table" << std::endl;
		return -1;
	}
}

extern "C" int fdht_del(char *key)
{
	return 0;
}

extern "C" void fdht_clean(void)
{
	fd_table.clear();
}

/****************** fd hash table ******************/

extern "C" int opc_put(char *key, int val)
{
	int ret;
	std::string k = key;
	openc_table.insert(k, val);
	return 0;
}

extern "C" int opc_update(char *key, int val)
{
	return 0;
}

extern "C" int opc_get(char *key)
{
	int ret;
	std::string k = key;
	if (openc_table.find(k, ret))
	{
		return ret;
	} else {
		//std::cout << "key not existed in dir check table" << std::endl;
		return -1;
	}
}

extern "C" int opc_del(char *key)
{
	return 0;
}

extern "C" void opc_clean(void)
{
	openc_table.clear();
}

