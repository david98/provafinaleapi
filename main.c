#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include "xxhash.h"
#include <time.h>

#define MAX_LINE_LENGTH 200
#define INITIAL_MON_REL_SIZE 512
#define INITIAL_MON_ENT_SIZE 2097152
#define INITIAL_HASH_TABLE_SIZE 256
#define HT_RESIZE_THRESHOLD_PERCENTAGE 50

#define ACTION_ADD_ENT "addent"
#define ACTION_DEL_ENT "delent"
#define ACTION_ADD_REL "addrel"
#define ACTION_DEL_REL "delrel"
#define ACTION_REPORT "report"

#define MAX_PARAM_LENGTH 40
#define MAX_PARAMS 4

#define DOUBLE_HASHING_FACTOR 1

#define MAX_ENTITIES_NUMBER 100000
#define MAX_RELATIONSHIPS_NUMBER 100000

#define DA_RESIZE_THRESHOLD_PERCENTAGE 98
#define DA_GROWTH_FACTOR 2
#define INITIAL_DA_SIZE 100

static const int dummy = 1;

int compare_strings(const void *a, const void *b) {
    const char *pa = *(const char **) a;
    const char *pb = *(const char **) b;

    return strcmp(pa, pb);
}

static unsigned long inline
djb2(unsigned char *str) {
    unsigned long hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* djb2 * 33 + c */

    return hash;
}

static unsigned long inline
sdbm(str)
        unsigned char *str;
{
    unsigned long hash = 0;
    int c;

    while ((c = *str++))
        hash = c + (hash << 6) + (hash << 16) - hash;

    return hash;
}

static unsigned long long inline calcul_hash(const void* buffer)
{
    return djb2(buffer);
}

struct ht_item {
    char *key;
    unsigned long long int hash;
    void *value;
};

static const struct ht_item HT_DELETED_ITEM = {NULL, 0, NULL};

struct hash_table {
    struct ht_item **array;
    unsigned long int size;
    unsigned long int count;
};

void ht_init(struct hash_table *ht, unsigned long int initial_size) {
    ht->array = calloc(initial_size, sizeof(struct ht_item *));
    if (ht->array == NULL) {
        exit(1);
    }
    ht->size = initial_size;
    ht->count = 0u;
}

struct hash_table *ht_new(unsigned long int initial_size) {
    struct hash_table *ht = malloc(sizeof(struct hash_table));
    if (ht == NULL) {
        exit(1);
    }
    ht_init(ht, initial_size);
    return ht;
}

unsigned long long int ht_get_index(struct hash_table *ht, char *key, int double_hashing_round) {
    unsigned long long int a = calcul_hash(key);
    unsigned long int b = 0;
    if (double_hashing_round > 0){
        b = sdbm(key);
    }
    unsigned long long int value = (a + double_hashing_round * (b + 1));
    value = value < 0 ? -value : value;
    unsigned long long int index = value & (ht->size - 1);

    return index;
}

int ht_insert(struct hash_table *ht, char *key, void *elem);

int ht_insert_no_resize(struct hash_table *ht, char *key, void *elem);

void ht_destroy(struct hash_table *ht);

void ht_resize(struct hash_table *ht, size_t new_size) {
    struct ht_item **old_array = ht->array;
    ht->array = calloc(new_size, sizeof(struct ht_item *));
    if (ht->array == NULL) {
        ht_destroy(ht);
        exit(1);
    } else {
        size_t old_size = ht->size;
        ht->size = new_size;
        unsigned long int old_count = ht->count;
        for (size_t i = 0; i < old_size; i++) {
            if (old_array[i] != NULL && old_array[i] != &HT_DELETED_ITEM) {
                ht_insert_no_resize(ht, old_array[i]->key, old_array[i]->value);
                free(old_array[i]->key);
                free(old_array[i]);
            }
        }
        ht->count = old_count;
        free(old_array);
    }
}

struct ht_item *ht_new_item(char *key, void *value) {
    struct ht_item *item = malloc(sizeof(struct ht_item));
    if (item == NULL) {
        exit(1);
    } else {
        item->key = strdup(key);
        if (item->key == NULL) {
            free(item);
            exit(1);
        } else {
            item->value = value;
            item->hash = calcul_hash(key);
        }
    }
    return item;
}

/*
 * Returns 0 if elem was not already in ht, 1 otherwise (replacement)
 * */
int __ht_insert(struct hash_table *ht, char *key, void *elem, short int resizing) {
    struct ht_item *item = ht_new_item(key, elem);
    int return_value = 0;
    if (resizing) {
        if (ht->count >= ht->size * HT_RESIZE_THRESHOLD_PERCENTAGE / 100) {
            ht_resize(ht, (unsigned long int) ht->size * 2);
        } else if (ht->count <= (ht->size * 10 / 100)) {
            //ht_resize(ht, (unsigned long int)ht->size / 2);
        }
    }

    unsigned long int index = ht_get_index(ht, item->key, 0);
    unsigned long int max_double_hashing_rounds = ht->size / DOUBLE_HASHING_FACTOR;
    int i = 1;
    // try MAX_DOUBLE_HASHING_ROUNDS times to find a free spot with double hashing
    while (i < max_double_hashing_rounds && ht->array[index] != NULL &&
           ht->array[index] != &HT_DELETED_ITEM &&
           ht->array[index]->hash != item->hash &&
           strcmp(item->key, ht->array[index]->key) != 0) {
        index = ht_get_index(ht, item->key, i);
        i++;
    }

    int j = 1;
    while (ht->array[index] != NULL &&
           ht->array[index] != &HT_DELETED_ITEM &&
           ht->array[index]->hash != item->hash &&
           strcmp(item->key, ht->array[index]->key) != 0) {
        /* we failed with double hashing, we switch to a different strategy:
         * we linear probe from index to the end and then from start to end
         * (we are guaranteed to find a free spot because we double table size
         *  whenever there's just one left */
        index += j * j;
        j++;
        if (index >= ht->size) {
            index = 0;
        }
    }
    if (ht->array[index] != NULL && ht->array[index] != &HT_DELETED_ITEM) {
        return_value = 1;
        free(ht->array[index]->key);
        free(ht->array[index]);
    } else {
        ht->count++;
    }
    ht->array[index] = item;
    return return_value;
}

/*
 * Returns 0 if elem was not already in ht, 1 otherwise (replacement)
 * */
int ht_insert_no_resize(struct hash_table *ht, char *key, void *elem) {
    return __ht_insert(ht, key, elem, 0);
}

/*
 * Returns 0 if elem was not already in ht, 1 otherwise (replacement)
 * */
int ht_insert(struct hash_table *ht, char *key, void *elem) {
    return __ht_insert(ht, key, elem, 1);
}

void *ht_get(struct hash_table *ht, char *key) {
    unsigned long int index = ht_get_index(ht, key, 0);
    struct ht_item *item = ht->array[index];
    int i = 1;
    unsigned long int hash = calcul_hash(key);
    short int half_probed = 0;
    unsigned long int max_double_hashing_rounds = ht->size / DOUBLE_HASHING_FACTOR;

    while (i < max_double_hashing_rounds && item != NULL) {
        if (item != &HT_DELETED_ITEM && hash == item->hash && strcmp(item->key, key) == 0) {
            return item->value;
        }

        index = ht_get_index(ht, key, i);
        item = ht->array[index];
        i++;
    }

    /* Same as above, if we failed to find the item we linear probe
     * */
    int j = 1;
    while ((index < ht->size || !half_probed) && item != NULL) {
        if (item != &HT_DELETED_ITEM && hash == item->hash && strcmp(item->key, key) == 0) {
            return item->value;
        }
        index += j * j;
        j++;
        if (index >= ht->size) {
            index = 0;
            half_probed = 1;
        }
        item = ht->array[index];
    }

    return NULL;
}

/*
 * Returns 0 if no element was deleted, 1 otherwise
 * */
int ht_delete(struct hash_table *ht, char *key) {
    unsigned long int index = ht_get_index(ht, key, 0);
    unsigned long int hash = calcul_hash(key);
    int i = 1;
    unsigned long int max_double_hashing_rounds = ht->size / DOUBLE_HASHING_FACTOR;

    // try MAX_DOUBLE_HASHING_ROUNDS times to find a free spot with double hashing
    while (i < max_double_hashing_rounds && ht->array[index] != NULL) {
        if (ht->array[index] != &HT_DELETED_ITEM && ht->array[index]->hash == hash &&
            strcmp(key, ht->array[index]->key) == 0) {

            free(ht->array[index]->key);
            free(ht->array[index]);
            ht->array[index] = &HT_DELETED_ITEM;
            ht->count--;
            return 1;
        }
        index = ht_get_index(ht, key, i);
        i++;
    }

    short int half_probed = 0;
    int j = 1;
    while ((index < ht->size || !half_probed) && ht->array[index] != NULL) {
        if (ht->array[index] != &HT_DELETED_ITEM && ht->array[index]->hash == hash &&
            strcmp(key, ht->array[index]->key) == 0) {

            free(ht->array[index]->key);
            free(ht->array[index]);
            ht->array[index] = &HT_DELETED_ITEM;
            ht->count--;
            return 1;
        }
        index += j * j;
        j++;
        if (index >= ht->size) {
            index = 0;
            half_probed = 1;
        }
    }

    return 0;
}

void print_keys(struct hash_table *ht) {
    printf("\n[");
    for (int i = 0; i < ht->size; i++) {
        if (ht->array[i] != NULL && ht->array[i] != &HT_DELETED_ITEM)
            printf(" '%s',", ht->array[i]->key);
    }
    printf("]\n");
}

void ht_destroy(struct hash_table *ht) {
    for (int i = 0; i < ht->size; i++) {
        if (ht->array[i] != NULL && ht->array[i] != &HT_DELETED_ITEM) {
            free(ht->array[i]->key);
            if (ht->array[i]->value != &dummy)
                free(ht->array[i]->value);
            free(ht->array[i]);
        }
    }
    free(ht->array);
    free(ht);
}

struct list_node {
    struct list_node *prev;
    struct list_node *next;
    void *elem;
};

struct list {
    struct list_node *head;
    struct list_node *tail;
    int length;
};

void list_init(struct list *list) {
    list->head = NULL;
    list->tail = NULL;
    list->length = 0;
}

struct list_node *list_create_new_node(void *elem, size_t elem_size) {
    struct list_node *node = malloc(sizeof(struct list_node));
    if (node == NULL) {
        exit(1);
    }
    node->prev = NULL;
    node->next = NULL;
    node->elem = malloc(elem_size);
    if (node->elem == NULL) {
        free(node);
        exit(1);
    }
    memcpy(node->elem, elem, elem_size);
    return node;
}

struct list *list_new() {
    struct list *ls = malloc(sizeof(struct list));
    list_init(ls);
    return ls;
}

void list_append(struct list *list, void *elem, size_t elem_size) {
    struct list_node *new_node = list_create_new_node(elem, elem_size);
    if (list->tail == NULL) {
        list->head = new_node;
        list->tail = new_node;
    } else {
        new_node->prev = list->tail;
        list->tail->next = new_node;
        list->tail = new_node;
    }
    list->length++;
}

void list_remove(struct list *list, void *elem, int (*cmp)(void *, void *)) {
    struct list_node *cur = list->head;
    while (cur != NULL) {
        if (cur->elem != NULL && cmp(cur->elem, elem) == 0) {
            if (cur == list->head) {
                if (cur == list->tail) {
                    list->head = NULL;
                    list->tail = NULL;
                    free(cur->elem);
                    free(cur);
                } else {
                    list->head = cur->next;
                    free(cur->elem);
                    free(cur);
                }
            } else if (cur == list->tail) {
                list->tail = cur->prev;
                list->tail->next = NULL;
                free(cur->elem);
                free(cur);
            } else {
                cur->prev->next = cur->next;
                cur->next->prev = cur->prev;
                free(cur->elem);
                free(cur);
            }
            list->length--;
            return;
        }
        cur = cur->next;
    }
}


void list_print(struct list *list) {
    struct list_node *cur = list->head;
    printf("[");
    while (cur != NULL) {
        if (cur->next != NULL) {
            printf("'%s', ", (char *) cur->elem);
        } else {
            printf("'%s'", (char *) cur->elem);
        }
        cur = cur->next;
    }
    printf("]\n");
}

void list_destroy(struct list *list) {
    struct list_node *cur = list->head;
    while (cur != NULL) {
        struct list_node *next = cur->next;
        free(cur->elem);
        free(cur);
        cur = next;
    }
    free(list);
}

struct din_arr {
    void **array;
    unsigned long int next_free;
    size_t size;
};

struct din_arr* din_arr_new(size_t initial_size){
    struct din_arr *arr = malloc(sizeof(struct din_arr));
    if (arr == NULL){
        exit(666);
    }
    arr->array = calloc(initial_size, sizeof(void *));
    if (arr->array == NULL){
        exit(666);
    }
    arr->size = initial_size;
    arr->next_free = 0;
    return arr;
}

size_t din_arr_resize(struct din_arr *arr, size_t new_size){
    if (new_size <= arr->size){
        return arr->size;
    }
    arr->array = realloc(arr->array, sizeof(void*) * new_size);
    if (arr->array == NULL){
        exit(666);
    }
    arr->size = new_size;
    return new_size;
}

void din_arr_append(struct din_arr *arr, void *elem, size_t elem_size){
    if (arr->next_free >= arr->size * DA_RESIZE_THRESHOLD_PERCENTAGE / 100){
        size_t old_size = arr->size;
        size_t new_size = din_arr_resize(arr, arr->size * DA_GROWTH_FACTOR);
        if (new_size <= old_size){
            exit(666);
        }
    }
    arr->array[arr->next_free] = malloc(elem_size);
    memcpy(arr->array[arr->next_free], elem, elem_size);
    arr->next_free++;
}

void din_arr_remove(struct din_arr *arr, void *elem, int (*cmp)(void *, void *)) {
    for(size_t i = 0; i < arr->next_free; i++){
        if( cmp(arr->array[i], elem) == 0){
            free(arr->array[i]);
            arr->array[i] = arr->array[--arr->next_free];
        }
    }
}

void din_arr_sort(struct din_arr *arr, int (*cmp)(const void *a, const void *b)){
    qsort(arr->array, arr->next_free, sizeof(void *), cmp);
}

void din_arr_destroy(struct din_arr *arr){
    size_t i;
    for (i = 0; i < arr->size; i++){
        free(arr->array[i]);
    }
    free(arr);
}

void add_ent(char *entity_name, struct hash_table *mon_ent, struct din_arr *mon_ent_list) {
    /*
     * Check if entity_name is being monitored
     * */
    if (!ht_insert(mon_ent, entity_name, (void *) &dummy)) {
        /*
         * If not, start monitoring it
         * */
        din_arr_append(mon_ent_list, entity_name, (strlen(entity_name) + 1) * sizeof(char));
    }
}

void add_rel(char *origin_ent, char *dest_ent, char *rel_name, struct hash_table *mon_ent,
             struct hash_table *mon_rel, struct din_arr *mon_rel_list) {
    /*
     * Check if both origin_ent and dest_ent
     * are being monitored
     * */
    if (ht_get(mon_ent, origin_ent) != NULL && ht_get(mon_ent, dest_ent) != NULL) {
        /*
         * Try to retrieve the hash table for rel_name
         * */
        struct hash_table *rel_table = ht_get(mon_rel, rel_name);
        if (rel_table == NULL) {
            /*
             * If we get here, rel_name was not being monitored:
             * we instantiate a new hash table for it and insert
             * it into mon_rel
             * */
            rel_table = ht_new(INITIAL_HASH_TABLE_SIZE);
            ht_insert(mon_rel, rel_name, rel_table);
            din_arr_append(mon_rel_list, rel_name, sizeof(char) * (strlen(rel_name) + 1));
        }
        /*
         * We try to retrieve the hash table containing all entities
         * that are in rel_name with dest_ent
         * */
        struct hash_table *dest_table = ht_get(rel_table, dest_ent);
        if (dest_table == NULL) {
            /*
             * If we're here, origin_ent is the first entity
             * to be in rel_name with dest_ent, so we create
             * a new hash_table and insert into the table for
             * rel_name
             * */
            dest_table = ht_new(INITIAL_HASH_TABLE_SIZE);
            ht_insert(rel_table, dest_ent, dest_table);
        }
        /*
         * We insert a flag in the table for dest_ent
         * */
        ht_insert(dest_table, origin_ent, (void *) &dummy);
    }
}

void del_ent(char *entity_name, struct hash_table *mon_ent, struct din_arr *mon_ent_list,
             struct hash_table *mon_rel, struct din_arr *mon_rel_list) {
    /*
     * Check if entity_name is currently monitored and remove it
     * */
    if (ht_delete(mon_ent, entity_name)) {
        din_arr_remove(mon_ent_list, entity_name, strcmp);
        struct hash_table *rel_table;
        struct din_arr *rels_to_remove = din_arr_new(INITIAL_DA_SIZE);
        /*
        * Delete entity_name from all relationships
        * */
        for (unsigned long int i = 0; i < mon_rel_list->next_free; i++){
            char *cur_rel = mon_rel_list->array[i];
            rel_table = ht_get(mon_rel, cur_rel);
            if (rel_table != NULL) {
                /*
                 * Delete all relationships towards entity_name
                 * */
                struct hash_table *dest_table = ht_get(rel_table, entity_name);
                if (dest_table != NULL) {
                    ht_destroy(dest_table);
                    ht_delete(rel_table, entity_name);
                }
                /*
                 * Delete all relationships from entity_name
                 * */
                for (unsigned long int j = 0; j < mon_ent_list->next_free; j++){
                    char *ent = mon_ent_list->array[j];
                    dest_table = ht_get(rel_table, ent);
                    if (dest_table != NULL) {
                        ht_delete(dest_table, entity_name);
                        if (dest_table->count == 0) {
                            ht_delete(rel_table, ent);
                            ht_destroy(dest_table);
                        }
                    }
                }
                /*
                 * If the relationship table is now empty,
                 * mark it for removal from monitored relationships
                 * */
                if (rel_table->count == 0) {
                    din_arr_append(rels_to_remove, cur_rel, sizeof(char) * (strlen(cur_rel) + 1));
                }
            }
        }
        /*
         * Remove all relationships marked for removal
         * */
        for (size_t idx = 0; idx < rels_to_remove->next_free; idx++){
            rel_table = ht_get(mon_rel, rels_to_remove->array[idx]);
            ht_destroy(rel_table);
            ht_delete(mon_rel, rels_to_remove->array[idx]);
            din_arr_remove(mon_rel_list, rels_to_remove->array[idx], strcmp);
        }
        din_arr_destroy(rels_to_remove);
    }
}

void del_rel(char *origin_ent, char *dest_ent, char *rel_name,
             struct hash_table *mon_rel, struct din_arr *mon_rel_list) {
    struct hash_table *rel_table = ht_get(mon_rel, rel_name);
    /*
     * Check if rel_name is in mon_rel
     * */
    if (rel_table != NULL) {
        struct hash_table *dest_table = ht_get(rel_table, dest_ent);
        /*
         * Check if there's any "arrow"
         * going to dest_ent
         * */
        if (dest_table != NULL) {
            /*
             * If there's an "arrow" from origin_ent
             * to dest_ent, delete it
             * */

            ht_delete(dest_table, origin_ent);
            /*
             * If there's no other "arrow" going to dest_ent,
             * remove it from rel_table
             * */
            if (dest_table->count == 0) {
                ht_destroy(dest_table);
                ht_delete(rel_table, dest_ent);
                /*
                 * If rel_table is now empty (there was just that one "arrow"),
                 * delete it and remove rel_name from mon_rel
                 * */
                if (rel_table->count == 0) {
                    ht_destroy(rel_table);
                    ht_delete(mon_rel, rel_name);
                    din_arr_remove(mon_rel_list, rel_name, strcmp);
                }
            }
        }
    }
}

void report(struct din_arr *mon_ent_list, struct hash_table *mon_rel, struct din_arr *mon_rel_list) {
    if (mon_rel_list->next_free == 0) {
        printf("none\n");
    } else {
        /*
         * Sort mon_rel_list in ascending alphabetical order
         * */
        din_arr_sort(mon_rel_list, compare_strings);

        /*
         * Iterate on the now ordered array of all
         * monitored relationships
         * */
        for (unsigned long int j = 0; j < mon_rel_list->next_free; j++) {
            char *cur_rel = mon_rel_list->array[j];
            /*
             * best_ents_arr will hold the entities with the most
             * incoming "arrows" for rels[j]
             * */
            char *best_ents_arr[MAX_ENTITIES_NUMBER];
            int best_ents_arr_len = 0;
            unsigned long int count = 0;
            struct hash_table *rel_table = ht_get(mon_rel, cur_rel);
            for (unsigned long int i = 0; i < mon_ent_list->next_free; i++){
                char *ent = mon_ent_list->array[i];
                struct hash_table *dest_table = ht_get(rel_table, ent);
                if (dest_table != NULL && dest_table->count >= count) {
                    if (dest_table->count > count) {
                        best_ents_arr_len = 0;
                        count = dest_table->count;
                    }
                    best_ents_arr[best_ents_arr_len] = ent;
                    best_ents_arr_len++;

                }
            }
            /*
             * Sort best_ents_arr in ascending alphabetical order
             */
            qsort(best_ents_arr, best_ents_arr_len, sizeof(char *), compare_strings);

            printf("\"%s\" ", cur_rel);
            for (int i = 0; i < best_ents_arr_len; i++) {
                printf("\"%s\" ", best_ents_arr[i]);
            }
            printf("%ld;", count);
            if (j + 1 < mon_rel_list->next_free) {
                printf(" ");
            }
        }
        printf("\n");
    }

}

int main(void) {
    /*struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);*/
    freopen("input.txt", "r", stdin);
    freopen("output.txt", "w", stdout);

    struct hash_table *mon_ent, *mon_rel;
    struct din_arr *mon_ent_list, *mon_rel_list;
    char line[MAX_LINE_LENGTH] = "", filtered_line[MAX_LINE_LENGTH] = "";

    mon_ent = ht_new(INITIAL_MON_ENT_SIZE);
    mon_rel = ht_new(INITIAL_MON_REL_SIZE);


    mon_ent_list = din_arr_new(INITIAL_MON_ENT_SIZE);
    mon_rel_list = din_arr_new(INITIAL_MON_REL_SIZE);

    const char *action_add_ent = ACTION_ADD_ENT;
    const char *action_del_ent = ACTION_DEL_ENT;
    const char *action_add_rel = ACTION_ADD_REL;
    const char *action_del_rel = ACTION_DEL_REL;
    const char *action_report = ACTION_REPORT;

    char *action = NULL;
    char *param1 = NULL, *param2 = NULL, *param3 = NULL;

    char **params = calloc(MAX_PARAMS, sizeof(char *));
    if (params == NULL){
        exit(666);
    }

    while (fgets(line, MAX_LINE_LENGTH, stdin)) {
        int n_par = 0;
        size_t line_len = strlen(line);
        size_t filt_len = 0;
        for (size_t i = 0; i < line_len; i++){
            if (line[i] != '\"' && line[i] != '\n' && line[i] != '\r'){
                filtered_line[filt_len] = line[i];
                filt_len++;
            }
        }
        filtered_line[filt_len] = '\0';
        char *token = strtok(filtered_line, " ");
        int valid = 1;
        while (token != NULL) {
            if (n_par >= MAX_PARAMS){
                valid = 0;
                break;
            }
            params[n_par] = token;
            token = strtok(NULL, " ");
            n_par++;
        }
        if (valid) {

            action = params[0];
            param1 = params[1];
            param2 = params[2];
            param3 = params[3];
            //printf("command: %s %s %s %s\n", action, param1, param2, param3);

            if (strcmp(action, action_add_ent) == 0) {
                if ((param1 != NULL && param1[0] != '\0') &&
                    (param2 == NULL || param2[0] == '\0')
                    && (param3 == NULL || param3[0] == '\0')) {
                    add_ent(param1, mon_ent, mon_ent_list);
                }
            } else if (strcmp(action, action_del_ent) == 0) {
                if ((param1 != NULL && param1[0] != '\0') &&
                    (param2 == NULL || param2[0] == '\0')
                    && (param3 == NULL || param3[0] == '\0')) {
                    del_ent(param1, mon_ent, mon_ent_list, mon_rel, mon_rel_list);
                }
            } else if (strcmp(action, action_add_rel) == 0) {
                if ((param1 != NULL && param1[0] != '\0') &&
                    (param2 != NULL && param2[0] != '\0')
                    && (param3 != NULL && param3[0] != '\0')) {
                    add_rel(param1, param2, param3, mon_ent, mon_rel, mon_rel_list);
                }
            } else if (strcmp(action, action_del_rel) == 0) {
                if ((param1 != NULL && param1[0] != '\0') &&
                    (param2 != NULL && param2[0] != '\0')
                    && (param3 != NULL && param3[0] != '\0')) {
                    del_rel(param1, param2, param3, mon_rel, mon_rel_list);
                }
            } else if (strcmp(action, action_report) == 0) {
                if ((param1 == NULL || param1[0] == '\0') &&
                        (param2 == NULL || param2[0] == '\0')
                        && (param3 == NULL || param3[0] == '\0')) {
                    report(mon_ent_list, mon_rel, mon_rel_list);
                }
            } else if (strcmp(action, "end") == 0) {
                goto END;
            }
        }

        memset(line, 0, MAX_LINE_LENGTH * sizeof(char));
        memset(filtered_line, 0, MAX_LINE_LENGTH * sizeof(char));
        memset(params, 0, MAX_PARAMS * sizeof(char *));
    }

    // free all memory (or not, lol)
    END:
    /*clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    uint64_t delta_us = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
    printf("%f ms", (double)delta_us/1000);*/

    exit(0);
}