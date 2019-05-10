CC = gcc
PROM = pacon_server
SOURCE = pacon_server.c
$(PROM): $(SOURCE)
	$(CC) -Wall -g -lm -o $(PROM) $(SOURCE) -lzmq -lpthread `--cflags --libs`
