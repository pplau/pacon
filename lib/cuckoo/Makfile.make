CC = g++
PROM = cuckoo_test
SOURCE = c_hash.c blob_blob_table.cc int_str_table.cc ./libcuckoo-c/cuckoo_table_template.cc
$(PROM): $(SOURCE)
	$(CC) -Wall -g -lm -lpthread -o $(PROM) $(SOURCE)
