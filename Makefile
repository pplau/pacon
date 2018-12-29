CC = gcc
#PROM = test
PROM = otter
#SOURCE = ./fs/test_fs.c ./fs/fs.c ./fs/hashmap.c ./fs/dcache.c ./fs/splitmerge.c ./lib/queue.c ./lib/queue_internal.c ./fs/mdstore.c
SOURCE = main.c ./lib/queue.c ./lib/queue_internal.c ./fs/mdstore.c ./lib/bloom.c ./lib/murmur.c
$(PROM): $(SOURCE)
	$(CC) -Wall -g -lm -o $(PROM) $(SOURCE) `pkg-config fuse --cflags --libs`
