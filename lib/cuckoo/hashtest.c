
#include <stdio.h>
#include "./hash_table.h"

int main(int argc, char const *argv[])
{
	int ret;
	char *path = "/a/b";
	int val = 11211;
	pdirht_put(path, val);
	ret = pdirht_get(path);
	printf("val = %d\n", ret);
	return 0;
}