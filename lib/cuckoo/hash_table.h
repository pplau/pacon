/*
 *  written by Yubo
 */
#ifndef HASH_H
#define HASH_H


int pdirht_put(char *key, int val);

int pdirht_update(char *key, int val);

int pdirht_get(char *key);

int hpdirht_del(char *key);


int fdht_put(char *key, int val);

int fdht_update(char *key, int val);

int fdht_get(char *key);

int fdht_del(char *key);


#endif