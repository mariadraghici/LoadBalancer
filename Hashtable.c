/* Copyright 2021 <Draghici Maria> */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"

#include "Hashtable.h"
#include "LinkedList.h"
#include "utils.h"

#define MAX_BUCKET_SIZE 200

// Functii de hash a obiectelor:
unsigned int hash_function_key(void *a) {
    unsigned char *puchar_a = (unsigned char *) a;
    unsigned int hash = 5381;
    int c;

    while ((c = *puchar_a++))
        hash = ((hash << 5u) + hash) + c;

    return hash;
}

int
compare_function_ints(void *a, void *b)
{
	int int_a = *((int *)a);
	int int_b = *((int *)b);

	if (int_a == int_b) {
		return 0;
	} else if (int_a < int_b) {
		return -1;
	} else {
		return 1;
	}
}

int
compare_function_strings(void *a, void *b)
{
	char *str_a = (char *)a;
	char *str_b = (char *)b;

	return strcmp(str_a, str_b);
}

// Functie apelata dupa alocarea unui hashtable pentru a-l initializa.
hashtable_t *
ht_create(unsigned int hmax, unsigned int (*hash_function)(void*),
		int (*compare_function)(void*, void*))
{
	unsigned int i;

	hashtable_t *ht = malloc(sizeof(hashtable_t));

	ht->buckets = malloc(hmax * sizeof(linked_list_t*));

	for (i = 0; i < hmax; i++) {
		ht->buckets[i] = ll_create(sizeof(struct info));
	}

	ht->hmax = hmax;
	ht->size = 0;
	ht->compare_function = compare_function;
	ht->hash_function = hash_function;

	return ht;
}

// Functie de gasire a unei chei si a pozitiei acesteia
ll_node_t*
find_key(linked_list_t *bucket, void *key,
	int (*compare_function)(void*, void*), unsigned int *pos)
{
	unsigned int i;
	ll_node_t *item = bucket->head;
    for (i = 0; i < bucket->size; ++i) {
    	int ok = compare_function(key, ((struct info*)item->data)->key);
    	if (ok == 0) {
    		*pos = i;
    		return item;
    	}
    	item = item->next;
    }
    return NULL;
}

void
ht_put(hashtable_t *ht, void *key, unsigned int key_size,
	void *value, unsigned int value_size)
{
	unsigned int pos = 0;
	struct info data;
	unsigned int index = ht->hash_function(key) % ht->hmax;
	linked_list_t *bucket = ht->buckets[index];
	ll_node_t *item = find_key(bucket, key, ht->compare_function, &pos);

	if (item == NULL) {
		data.key = malloc(key_size);
		data.value = malloc(value_size);
		memcpy(data.key, key, key_size);
		memcpy(data.value, value, value_size);
		ll_add_nth_node(bucket, 0, &data);
		ht->size++;
	} else {
		free(((struct info*)item->data)->value);
		((struct info*)item->data)->value = malloc(value_size);
		memcpy(((struct info*)item->data)->value, value, value_size);
	}
}

void *
ht_get(hashtable_t *ht, void *key)
{
	unsigned int pos = 0;
	unsigned int index = ht->hash_function(key) % ht->hmax;
	linked_list_t *bucket = ht->buckets[index];
	ll_node_t *item = find_key(bucket, key, ht->compare_function, &pos);
	if (item)
		return ((struct info*)item->data)->value;
	return NULL;
}

/* Functie care intoarce:
 * 1, daca pentru cheia key a fost asociata anterior o valoare in hashtable 
 folosind functia put 
 * 0, altfel.
 */
int
ht_has_key(hashtable_t *ht, void *key)
{
	unsigned int pos = 0;
	unsigned int index = ht->hash_function(key) % ht->hmax;
	linked_list_t *bucket = ht->buckets[index];
	ll_node_t *item = find_key(bucket, key, ht->compare_function, &pos);
	if (item != NULL)
		return 1;
	return 0;
}

// Procedura care elimina din hashtable intrarea asociata cheii key.
void
ht_remove_entry(hashtable_t *ht, void *key)
{
	unsigned int pos = 0;
	unsigned int index = ht->hash_function(key) % ht->hmax;
	linked_list_t *bucket = ht->buckets[index];
	find_key(bucket, key, ht->compare_function, &pos);
	ll_node_t *deleted = ll_remove_nth_node(bucket, pos);
	ht->size--;
	free(((struct info*)deleted->data)->value);
	free(((struct info*)deleted->data)->key);
	free(deleted->data);
	free(deleted);
}

/*
 * Procedura care elibereaza memoria folosita de toate intrarile din hashtable,
  dupa care elibereaza si memoria folosita pentru a stoca structura hashtable.
 */ 
void
ht_free(hashtable_t *ht)
{
	unsigned int i;
	for (i = 0; i < ht->hmax; ++i) {
		ll_node_t* item;
		while (ll_get_size(ht->buckets[i]) > 0) {
			item = ll_remove_nth_node(ht->buckets[i], 0);
			free(((struct info*)item->data)->value);
			free(((struct info*)item->data)->key);
			free(item->data);
			free(item);
		}

		free(ht->buckets[i]);
		ht->buckets[i] = NULL;
	}
	free(ht->buckets);
	free(ht);
}

unsigned int
ht_get_size(hashtable_t *ht)
{
	if (ht == NULL)
		return 0;

	return ht->size;
}

unsigned int
ht_get_hmax(hashtable_t *ht)
{
	if (ht == NULL)
		return 0;

	return ht->hmax;
}

hashtable_t *
ht_resize(hashtable_t *ht)
{
	hashtable_t *resized_ht = ht_create(ht->hmax * 2, ht->hash_function,
		ht->compare_function);

	unsigned int i;

	for (i = 0; i < ht->hmax; i++) {
		linked_list_t *bucket = ht->buckets[i];
		ll_node_t *item = bucket->head;
		while (item) {
			ht_put(resized_ht, ((struct info *)item->data)->key,
				strlen(((struct info *)item->data)->key) + 1,
				((struct info *)item->data)->value,
				strlen(((struct info *)item->data)->value) + 1);
			item = item->next;
		}
	}
	ht_free(ht);
	return resized_ht;
}

