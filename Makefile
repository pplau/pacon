CC = gcc
#PROM = test
PROM = pacon
#SOURCE = ./fs/test_fs.c ./fs/fs.c ./fs/hashmap.c ./fs/dcache.c ./fs/splitmerge.c ./lib/queue.c ./lib/queue_internal.c ./fs/mdstore.c
SOURCE = fs_test.c pacon.c ./kv/dmkv.c ./lib/cJSON.c ./lib/cuckoo/hash_table.cc
$(PROM): $(SOURCE)
	$(CC) -w -g -lm -std=c++11 -lstdc++ -o $(PROM) $(SOURCE) -lmemcached -lzmq -lpthread `pkg-config fuse --cflags --libs`
