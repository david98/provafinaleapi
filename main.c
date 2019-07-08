#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned long
djb2(unsigned char *str)
{
    unsigned long hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* djb2 * 33 + c */

    return hash;
}

static unsigned long
sdbm(str)
        unsigned char *str;
{
    unsigned long hash = 0;
    int c;

    while ((c = *str++))
        hash = c + (hash << 6) + (hash << 16) - hash;

    return hash;
}

struct ht_item{
    char *key;
    void *value;
};

struct hash_table{
    struct ht_item **array;
    long int size;
    long int count;
};

void ht_init(struct hash_table *ht, long int initial_size){
    ht->array = calloc(initial_size, sizeof(struct ht_item *));
    if (ht->array == NULL){
        exit(1);
    }
    ht->size = initial_size;
    ht->count = 0;
}

long int ht_get_index(struct hash_table *ht, char *key, int double_hashing_round){
    long int a = djb2(key);
    long int b = sdbm(key);
    long int value = (a + double_hashing_round * (b + 1));
    value = value < 0 ? -value : value;
    long int index = value & (ht->size - 1);

    return index;
}

void ht_rehash(struct hash_table *ht, struct ht_item **old_array, long int old_size){
    struct ht_item *cur;
    long int index = -1;

    for (int i = 0; i < old_size; i++) {
        cur = old_array[i];
        if (cur != NULL) {
            int i = 0;
            index = ht_get_index(ht, cur->key, 0);

            while (ht->array[index] != NULL) {
                index = ht_get_index(ht, cur->key, i);
                i++;
            }
            ht->array[index] = cur;
        }
    }

}

void ht_resize(struct hash_table *ht, long int new_size){
    struct ht_item **old_array = ht->array;
    ht->array = calloc(new_size, sizeof(struct ht_item*));
    if (ht->array == NULL){
        exit(1);
    } else {
        long int old_size = ht->size;
        ht->size = new_size;
        ht_rehash(ht, old_array, old_size);
        free(old_array);
    }
}

struct ht_item* ht_new_item(char *key, void *value){
    struct ht_item *item = malloc(sizeof(struct ht_item));
    if (item == NULL){
        exit(1);
    } else {
        item->key = malloc(sizeof(char) * strlen(key));
        if (item->key == NULL){
            exit(1);
        } else {
            strcpy(item->key, key);
            item->value = value;
        }
    }
    return item;
}

void ht_insert(struct hash_table *ht, char *key, void *elem){
    struct ht_item *item = ht_new_item(key, elem);

    ht->count++;
    if (ht->count == ht->size){
       ht_resize(ht, ht->size * 2);
    } else if (ht->count <= (ht->size/4)){
        ht_resize(ht, ht->size / 2);
    }

    long int index = ht_get_index(ht, item->key, 0);

    int i = 1;
    while (ht->array[index] != NULL && strcmp(item->key, ht->array[index]->key) != 0) {
        index = ht_get_index(ht, item->key, i);
        i++;
    }
    ht->array[index] = item;

}

void* ht_get(struct hash_table *ht, char *key){
    long int index = ht_get_index(ht, key, 0);
    struct ht_item *item = ht->array[index];
    int i = 1;
    while (i < ht->size && item != NULL) {
        if (strcmp(item->key, key) == 0){
            return item->value;
        }
        index = ht_get_index(ht, key, i);
        item = ht->array[index];
        i++;
    }
    return NULL;
}

int main() {

    struct hash_table ht;

    ht_init(&ht, 256);

    ht_insert(&ht, "FYFYFYEzFYFYFYEzFYEzFYFYFYFYFY", "ciao");
    ht_insert(&ht, "EzFYEzEzEzFYFYFYFYEzEzFYEzEzFY", "ciao2");
    ht_insert(&ht, "EzFYEzEzEzFYFYFYFYEzEzFYEzEzFY", "ciao3");

    void *elem = ht_get(&ht, "EzFYEzEzEzFYFYFYFYEzEzFYEzEzFY");
    if (elem != NULL) {
        char *string = (char *) elem;
        printf("\n\n%s", string);
    }
    return 0;
}