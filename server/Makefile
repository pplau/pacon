CC = gcc
PROM = pacon_server
SOURCE = pacon_server.c ../comm/comm.c ../kv/dmkv.c
$(PROM): $(SOURCE)
	$(CC) -w -g -lm -o $(PROM) $(SOURCE) -lzmq -lpthread -lmemcached `--cflags --libs`
