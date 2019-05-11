CC = g++
PROM = cuckoo_test
SOURCE = ./libcuckoo-c/cuckoo_table_template.cc blob_blob_table.cc int_str_table.cc c_hash.c
$(PROM): $(SOURCE)
	$(CC) -Wall -g -lm -lpthread -o $(PROM) $(SOURCE)