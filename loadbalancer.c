/* Copyright 2021 <Draghici Maria> */
#include <stdlib.h>
#include <string.h>

#include "load_balancer.h"

struct load_balancer {
    server_memory **servers;
    /* Numar total servere existente*/
    unsigned int servers_size;
    unsigned int *hashring;
    /* Capacitatile curente ale hashringului
    si vectorului de servere pentru a monotoriza
    necesitatea resize-ului */
    unsigned int hashring_capacity;
    unsigned int servers_capacity;
    /* Dimensiunea consumata*/
    unsigned int hashring_size;
    unsigned int objects_num;
};

unsigned int hash_function_servers(void *a) {
    unsigned int uint_a = *((unsigned int *)a);

    uint_a = ((uint_a >> 16u) ^ uint_a) * 0x45d9f3b;
    uint_a = ((uint_a >> 16u) ^ uint_a) * 0x45d9f3b;
    uint_a = (uint_a >> 16u) ^ uint_a;
    return uint_a;
}

/* Functia cauta binar in hashring o anumita eticheta (x-ul)*/
static
unsigned int binary_search_by_hash(unsigned int *array , unsigned int l,
                             unsigned int r, unsigned int x)
{
    if (r >= l) {
        unsigned int mid = l + (r - l) / 2;

        if (hash_function_servers(&(array[mid])) == x)
            return mid;
        if (hash_function_servers(&(array[mid])) > x)
            return binary_search_by_hash(array, l, mid - 1, x);

        return binary_search_by_hash(array, mid + 1, r, x);
    }

    return 0;
}

load_balancer* init_load_balancer() {
    load_balancer *balancer = calloc(1, sizeof(load_balancer));
    DIE(balancer == NULL, "load_balancer malloc");

    balancer->hashring_capacity = 3;
    balancer->servers_capacity = 1;

    balancer->hashring = calloc(balancer->hashring_capacity,
        sizeof(unsigned int));
    DIE(balancer->hashring == NULL, "load_balancer->hashring malloc");

    balancer->servers = calloc(balancer->servers_capacity,
        sizeof(server_memory *));
    DIE(balancer->servers == NULL, "load_balancer->servers malloc");

    balancer->servers_size = 0;
    balancer->hashring_size = 0;
    balancer->objects_num = 0;

    return balancer;
}

/* Functie ce cauta in vectorul de servere un anumit server
   dupa id */
static unsigned int
search_id(server_memory **array, unsigned int id, unsigned int size) {
    unsigned int i;

    for (i = 0; i < size; i++) {
        if (array[i]->id == id)
            return i;
    }

    return 0;
}

void loader_store(load_balancer* main, char* key, char* value, int* server_id)
{
    unsigned int i, pos, id;
    unsigned int hash = hash_function_key(key);

    for (i = 0; i < main->hashring_size; i++) {
        if (hash < hash_function_servers(&(main->hashring[i]))) {
            id = main->hashring[i] % 100000;
            pos = search_id(main->servers, id, main->servers_size);
            server_store(main->servers[pos], key, value);
            if (main->servers[pos]->ht->size == main->servers[pos]->ht->hmax)
                main->servers[pos]->ht = ht_resize(main->servers[pos]->ht);
            main->objects_num++;
            *server_id = (int)(main->hashring[i] % 100000);
            return;
        }
    }

    id = main->hashring[0] % 100000;
    pos = search_id(main->servers, id, main->servers_size);
    server_store(main->servers[pos], key, value);
    if (main->servers[pos]->ht->size == main->servers[pos]->ht->hmax)
         main->servers[pos]->ht = ht_resize(main->servers[pos]->ht);
    main->objects_num++;
    *server_id = (int)(main->hashring[0] % 100000);
}

char* loader_retrieve(load_balancer* main, char* key, int* server_id) {
    unsigned int i = 0, id, pos;
    int ok = 0;

    for (i = 0; i < main->hashring_size && ok == 0; i++) {
        id = main->hashring[i] % 100000;
        pos = search_id(main->servers, id, main->servers_size);
        if (server_retrieve(main->servers[pos], key)) {
            *server_id = (int)(main->hashring[i] % 100000);
            ok = 1;
        }
    }

    if (ok == 0)
       return NULL;

    return server_retrieve(main->servers[pos], key);
}

/* Functie ce sorteaza un vector de etichete
    in ordine crescatoare dupa hash */
static void
sort_labels_by_hash(unsigned int *label, unsigned int size)
{
    unsigned int i, j, label_copy = 0;

    // sort hash and label
    for (i = 0; i < size - 1; i++) {
        for (j = i + 1; j < size; j++) {
            if (hash_function_servers(&(label[i])) >
                hash_function_servers(&(label[j]))) {
                label_copy = label[i];
                label[i] = label[j];
                label[j] = label_copy;
            }

            if (hash_function_servers(&(label[i])) ==
                hash_function_servers(&(label[j]))
                && label[i] % 100000 > label[j] % 100000) {
                label_copy = label[i];
                label[i] = label[j];
                label[j] = label_copy;
            }
        }
    }
}

/* Functie ce cauta si returneaza id-ul severului care ar trebui
   sa stocheze o noua cheie adaugata, dar fara sa adauge elementul
   in server */
void
loader_search(load_balancer* main, char* key, int* server_id) {
    unsigned int i;
    unsigned int hash = hash_function_key(key);

    for (i = 0; i < main->hashring_size; i++) {
        if (hash < hash_function_servers(&(main->hashring[i]))) {
            *server_id = (int)(main->hashring[i] % 100000);
            return;
        }
    }

    *server_id = (int)(main->hashring[0] % 100000);
}

void
resize_hashring(load_balancer *main) {
    unsigned int i;

    if (main->hashring_size == main->hashring_capacity) {
        main->hashring_capacity *= 2;

        main->hashring = realloc(main->hashring,
                main->hashring_capacity * sizeof(unsigned int));
        DIE(main->hashring == NULL, "main->hashring realloc");
        for (i = main->hashring_size; i < main->hashring_capacity; i++)
            main->hashring[i] = 0;
    }
}

void
resize_servers(load_balancer *main) {
    unsigned int i;

    if (main->servers_size == main->servers_capacity) {
        main->servers_capacity *= 2;

        main->servers = realloc(main->servers,
                main->servers_capacity * sizeof(server_memory *));
        DIE(main->servers == NULL, "main->servers realloc");

        for (i = main->servers_size; i < main->servers_capacity; i++)
            main->servers[i] = 0;
    }
}

void
loader_add_server(load_balancer* main, int server_id) {
    unsigned int label[3], i, j = 0, pos, buckets_number,
    neigh_server, pos_neigh;
    int id;
    linked_list_t *bucket = NULL;
    ll_node_t *item = NULL, *item_copy = NULL;

    for (i = 0; i < 3; i++) {
        label[i] = i * 100000 + server_id;
    }

    // Sortare a etichetelor noului server
    sort_labels_by_hash(label, 3);

    // Adaugare etichete in hashring
    for (i = main->hashring_size; i < main->hashring_size + 3; i++) {
        main->hashring[i] = label[j];
        j++;
    }

    main->hashring_size = main->hashring_size + 3;
    resize_hashring(main);

    // Sortare hashring
    sort_labels_by_hash(main->hashring, main->hashring_size);
    main->servers[main->servers_size] = init_server_memory();
    main->servers[main->servers_size]->id = (unsigned int)server_id;
    main->servers_size++;
    resize_servers(main);

    /* Daca exista cel putin un server si un obiect
    se face redistribuirea necesara */
    if (main->servers_size > 1 && main->objects_num >= 1) {
        for (i = 0; i < 3; i++) {
            // Caut pozitia etichetei in hashring
            pos = binary_search_by_hash(main->hashring, 0,
                main->hashring_size - 1, hash_function_servers(&(label[i])));

            // Daca vecinul serverului nu este tot el
            if (main->hashring[pos + 1] % 100000 != (unsigned int)server_id) {
                if (pos < main->hashring_size - 1) {
                    neigh_server = main->hashring[pos + 1] % 100000;
                } else {
                    neigh_server = main->hashring[0] % 100000;
                }

                // Cauatre vecin in vectorul de servere
                pos_neigh = search_id(main->servers, neigh_server,
                    main->servers_size);
                buckets_number = main->servers[pos_neigh]->ht->hmax;

                // Parcurgere elemente din server vecin si redistribuirea lor
                for (j = 0; j < buckets_number; j++) {
                    bucket = main->servers[pos_neigh]->ht->buckets[j];
                    item = bucket->head;
                    while (item) {
                        item_copy = item->next;
                        loader_search(main,
                            (char *)((struct info*)item->data)->key, &id);
                        if ((unsigned int)id != neigh_server) {
                            loader_store(main,
                                (char *)((struct info*)item->data)->key,
                             (char *)((struct info*)item->data)->value, &id);
                            server_remove(main->servers[pos_neigh],
                                (char *)((struct info*)item->data)->key);
                        }
                        item = item_copy;
                    }
                }
            }
        }
    }
}

void loader_remove_server(load_balancer* main, int server_id) {
    unsigned int pos, label[3], buckets_number, i, q, pos_server;
    int id;
    ll_node_t *item;
    linked_list_t *bucket;

    for (i = 0; i < 3; i++) {
        label[i] = i * 100000 + server_id;
    }

    sort_labels_by_hash(label, 3);
    pos_server = search_id(main->servers, label[0] % 100000,
        main->servers_size);

    // Stergere etichete server de sters din hashring
    for (i = 0; i < 3; i++) {
        pos = binary_search_by_hash(main->hashring, 0,
            main->hashring_size - 1, hash_function_servers(&(label[i])));

        if (pos == main->hashring_size - 1) {
            main->hashring_size--;
        } else {
            for (q = pos; q < main->hashring_size - 1; q++)
                main->hashring[q] = main->hashring[q + 1];
            main->hashring_size--;
        }
    }

    // Parcurgere obiecte server sters si redistriubuire
    buckets_number = main->servers[pos_server]->ht->hmax;
    for (q = 0; q < buckets_number; q++) {
        bucket = main->servers[pos_server]->ht->buckets[q];
        item = bucket->head;
        while (item) {
            loader_store(main, (char *)((struct info*)item->data)->key,
                (char *)((struct info*)item->data)->value, &id);
            item = item->next;
        }
    }

    // Stergere server din vectorul de servere
    main->servers[pos_server]->id = 100000;
    if (pos_server == main->servers_size - 1) {
        free_server_memory(main->servers[pos_server]);
        main->servers_size--;
    } else {
        free_server_memory(main->servers[pos_server]);
        for (q = pos_server; q < main->servers_size - 1; q++)
            main->servers[q] = main->servers[q + 1];
        main->servers_size--;
    }
}

void free_load_balancer(load_balancer* main) {
    unsigned int i;

    for (i = 0; i < main->servers_size; i++) {
        free_server_memory(main->servers[i]);
    }

    free(main->servers);
    free(main->hashring);
    free(main);
}

