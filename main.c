#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
//#include <sys/time.h>

#define MAX_LINE_LENGTH 200
#define INITIAL_MON_REL_SIZE 256
#define INITIAL_MON_ENT_SIZE 2097152
#define INITIAL_HASH_TABLE_SIZE 32

#define ACTION_SIZE 6

#define ACTION_ADD_ENT "addent"
#define ACTION_DEL_ENT "delent"
#define ACTION_ADD_REL "addrel"
#define ACTION_DEL_REL "delrel"
#define ACTION_REPORT "report"

#define MAX_PARAMETER_SIZE 40

#define DOUBLE_HASHING_FACTOR 1

#define MAX_ENTITIES_NUMBER 100000
#define MAX_RELATIONSHIPS_NUMBER 100000

static const int dummy = 1;

int compare_strings(const void *a, const void *b) {
    const char *pa = *(const char **) a;
    const char *pb = *(const char **) b;

    return strcmp(pa, pb);
}

unsigned long
djb2(unsigned char *str) {
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

struct ht_item {
    char *key;
    unsigned long int djb2_hash;
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

unsigned long int ht_get_index(struct hash_table *ht, char *key, int double_hashing_round) {
    unsigned long int a = djb2(key);
    unsigned long int b = sdbm(key);
    unsigned long int value = (a + double_hashing_round * (b + 1));
    value = value < 0 ? -value : value;
    unsigned long int index = value & (ht->size - 1);

    return index;
}

void ht_insert(struct hash_table *ht, char *key, void *elem);

void ht_insert_no_resize(struct hash_table *ht, char *key, void *elem);

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
            item->djb2_hash = djb2(key);
        }
    }
    return item;
}

void __ht_insert(struct hash_table *ht, char *key, void *elem, short int resizing) {
    struct ht_item *item = ht_new_item(key, elem);

    if (resizing) {
        if (ht->count >= ht->size * 70 / 100) {
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
           ht->array[index]->djb2_hash != item->djb2_hash &&
           strcmp(item->key, ht->array[index]->key) != 0) {
        index = ht_get_index(ht, item->key, i);
        i++;
    }

    while (ht->array[index] != NULL &&
           ht->array[index] != &HT_DELETED_ITEM &&
           ht->array[index]->djb2_hash != item->djb2_hash &&
           strcmp(item->key, ht->array[index]->key) != 0) {
        /* we failed with double hashing, we switch to a different strategy:
         * we linear probe from index to the end and then from start to end
         * (we are guaranteed to find a free spot because we double table size
         *  whenever there's just one left */
        index++;
        if (index == ht->size) {
            index = 0;
        }
    }
    if (ht->array[index] != NULL && ht->array[index] != &HT_DELETED_ITEM) {
        free(ht->array[index]->key);
        free(ht->array[index]);
    } else {
        ht->count++;
    }
    ht->array[index] = item;
}

void ht_insert_no_resize(struct hash_table *ht, char *key, void *elem) {
    __ht_insert(ht, key, elem, 0);
}

void ht_insert(struct hash_table *ht, char *key, void *elem) {
    __ht_insert(ht, key, elem, 1);
}

void *ht_get(struct hash_table *ht, char *key) {
    unsigned long int index = ht_get_index(ht, key, 0);
    struct ht_item *item = ht->array[index];
    int i = 1;
    unsigned long int hash = djb2(key);
    short int half_probed = 0;
    unsigned long int max_double_hashing_rounds = ht->size / DOUBLE_HASHING_FACTOR;

    while (i < max_double_hashing_rounds && item != NULL) {
        if (item != &HT_DELETED_ITEM && hash == item->djb2_hash && strcmp(item->key, key) == 0) {
            return item->value;
        }

        index = ht_get_index(ht, key, i);
        item = ht->array[index];
        i++;
    }

    /* Same as above, if we failed to find the item we linear probe
     * */
    while ((index < ht->size || !half_probed) && item != NULL) {
        if (index == ht->size) {
            index = 0;
            half_probed = 1;
        }
        if (item != &HT_DELETED_ITEM && hash == item->djb2_hash && strcmp(item->key, key) == 0) {
            return item->value;
        }
        index++;
        item = ht->array[index];
    }

    return NULL;
}

void ht_delete(struct hash_table *ht, char *key) {
    unsigned long int index = ht_get_index(ht, key, 0);
    unsigned long int hash = djb2(key);
    int i = 1;
    unsigned long int max_double_hashing_rounds = ht->size / DOUBLE_HASHING_FACTOR;

    // try MAX_DOUBLE_HASHING_ROUNDS times to find a free spot with double hashing
    while (i < max_double_hashing_rounds && ht->array[index] != NULL) {
        if (ht->array[index] != &HT_DELETED_ITEM && ht->array[index]->djb2_hash == hash &&
            strcmp(key, ht->array[index]->key) == 0) {

            free(ht->array[index]->key);
            free(ht->array[index]);
            ht->array[index] = &HT_DELETED_ITEM;
            ht->count--;
            return;
        }
        index = ht_get_index(ht, key, i);
        i++;
    }

    short int half_probed = 0;

    while ((index < ht->size || !half_probed) && ht->array[index] != NULL) {
        if (ht->array[index] != &HT_DELETED_ITEM && ht->array[index]->djb2_hash == hash &&
            strcmp(key, ht->array[index]->key) == 0) {

            free(ht->array[index]->key);
            free(ht->array[index]);
            ht->array[index] = &HT_DELETED_ITEM;
            ht->count--;
            return;
        }
        index++;
        if (index == ht->size) {
            index = 0;
            half_probed = 1;
        }
    }
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

void add_ent(char *entity_name, struct hash_table *mon_ent, struct list *mon_ent_list) {
    /*
     * Check if entity_name is being monitored
     * */
    if (ht_get(mon_ent, entity_name) == NULL) {
        /*
         * If not, start monitoring it
         * */
        ht_insert(mon_ent, entity_name, (void *) &dummy);
        list_append(mon_ent_list, entity_name, (strlen(entity_name) + 1) * sizeof(char));
    }
}

void add_rel(char *origin_ent, char *dest_ent, char *rel_name, struct hash_table *mon_ent,
             struct hash_table *mon_rel, struct list *mon_rel_list) {
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
            list_append(mon_rel_list, rel_name, sizeof(char) * (strlen(rel_name) + 1));
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

void del_ent(char *entity_name, struct hash_table *mon_ent, struct list *mon_ent_list,
             struct hash_table *mon_rel, struct list *mon_rel_list) {
    /*
     * Check if entity_name is currently monitored
     * */
    if (ht_get(mon_ent, entity_name) != NULL) {
        /*
         * Remove it from monitored entities
         * */
        ht_delete(mon_ent, entity_name);
        list_remove(mon_ent_list, entity_name, strcmp);
        struct list_node *cur_rel = mon_rel_list->head;
        struct hash_table *rel_table;
        struct list *rels_to_remove = list_new();
        /*
        * Delete entity_name from all relationships
        * */
        while (cur_rel != NULL) {
            rel_table = ht_get(mon_rel, cur_rel->elem);
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
                struct list_node *ent = mon_ent_list->head;
                while (ent != NULL) {
                    dest_table = ht_get(rel_table, ent->elem);
                    if (dest_table != NULL) {
                        ht_delete(dest_table, entity_name);
                        if (dest_table->count == 0) {
                            ht_delete(rel_table, ent->elem);
                            ht_destroy(dest_table);
                        }
                    }
                    ent = ent->next;
                }
                /*
                 * If the relationship table is now empty,
                 * mark it for removal from monitored relationships
                 * */
                if (rel_table->count == 0) {
                    list_append(rels_to_remove, cur_rel->elem, sizeof(char) * (strlen(cur_rel->elem) + 1));
                }
            }
            cur_rel = cur_rel->next;
        }
        /*
         * Remove all relationships marked for removal
         * */
        struct list_node *rel = rels_to_remove->head;
        while (rel != NULL) {
            rel_table = ht_get(mon_rel, rel->elem);
            ht_destroy(rel_table);
            ht_delete(mon_rel, rel->elem);
            list_remove(mon_rel_list, rel->elem, strcmp);
            rel = rel->next;
        }
        list_destroy(rels_to_remove);
    }
}

void del_rel(char *origin_ent, char *dest_ent, char *rel_name,
             struct hash_table *mon_rel, struct list *mon_rel_list) {
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
                    list_remove(mon_rel_list, rel_name, strcmp);
                }
            }
        }
    }
}

void report(struct list *mon_ent_list, struct hash_table *mon_rel, struct list *mon_rel_list) {
    if (mon_rel_list->length == 0) {
        printf("none\n");
    } else {
        char *rels[MAX_RELATIONSHIPS_NUMBER];
        int rels_len = 0;
        struct list_node *rel = mon_rel_list->head;

        /*
         * Copy mon_rel_list in an array to be ordered
         * */
        while (rel != NULL) {
            rels[rels_len] = rel->elem;
            rels_len++;
            rel = rel->next;
        }

        /*
         * Sort rels in ascending alphabetical order
         * */
        qsort(rels, rels_len, sizeof(char *), compare_strings);

        /*
         * Iterate on the now ordered array of all
         * monitored relationships
         * */
        for (int j = 0; j < rels_len; j++) {
            char *cur_rel = rels[j];
            /*
             * best_ents_arr will hold the entities with the most
             * incoming "arrows" for rels[j]
             * */
            char *best_ents_arr[MAX_ENTITIES_NUMBER];
            int best_ents_arr_len = 0;
            int count = 0;
            struct hash_table *rel_table = ht_get(mon_rel, cur_rel);
            struct list_node *ent = mon_ent_list->head;
            while (ent != NULL) {
                struct hash_table *dest_table = ht_get(rel_table, ent->elem);
                if (dest_table != NULL && dest_table->count >= count) {
                    if (dest_table->count > count) {
                        best_ents_arr_len = 0;
                        count = dest_table->count;
                    }
                    best_ents_arr[best_ents_arr_len] = ent->elem;
                    best_ents_arr_len++;

                }
                ent = ent->next;
            }

            /*
             * Sort best_ents_arr in ascending alphabetical order
             */
            qsort(best_ents_arr, best_ents_arr_len, sizeof(char *), compare_strings);

            printf("\"%s\" ", cur_rel);
            for (int i = 0; i < best_ents_arr_len; i++) {
                printf("\"%s\" ", best_ents_arr[i]);
            }
            printf("%d;", count);
            if (j + 1 < rels_len) {
                printf(" ");
            }
        }
        printf("\n");
    }

}

int main(void) {
    //struct timespec start, end;
    //clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    freopen("input.txt", "r", stdin);
    freopen("output.txt", "w", stdout);
    struct hash_table *mon_ent, *mon_rel;
    struct list *mon_ent_list, *mon_rel_list;
    char line[MAX_LINE_LENGTH];

    mon_ent = ht_new(INITIAL_MON_ENT_SIZE);
    mon_rel = ht_new(INITIAL_MON_REL_SIZE);


    mon_ent_list = list_new();
    mon_rel_list = list_new();

    const char *action_add_ent = ACTION_ADD_ENT;
    const char *action_del_ent = ACTION_DEL_ENT;
    const char *action_add_rel = ACTION_ADD_REL;
    const char *action_del_rel = ACTION_DEL_REL;
    const char *action_report = ACTION_REPORT;

    char *action = NULL;
    char *param1 = NULL, *param2 = NULL, *param3 = NULL;

    while (fgets(line, MAX_LINE_LENGTH, stdin)) {
        int cur_par = 0;

        char *token = strtok(line, " ");
        short int valid = 1;
        while (token != NULL) {
            unsigned long int len = strlen(token);

            if (token[len - 1] == '\n'){
                token[len - 1] = '\0';
                len--;
            }
            if (token[0] == '"' && token[len - 1] == '"'){
                token++;
                token[len - 2] = '\0';
                len -= 2;
            } else if (cur_par > 0){
                valid = 0;
                break;
            }
            switch (cur_par) {
                case 0: {
                    action = token;
                    break;
                }
                case 1: {
                    param1 = token;
                    break;
                }
                case 2: {
                    param2 = token;
                    break;
                }
                case 3: {
                    param3 = token;
                    break;
                }
                default: {
                    break;
                }
            }
            token = strtok(NULL, " ");
            cur_par++;
        }

        if (cur_par >= 1 && cur_par <= 4 && action != NULL && valid) {
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
            } else if (strncmp(action, "end", 3) == 0) {
                goto END;
            }
        }

        memset(line, 0, MAX_LINE_LENGTH);
        action = NULL;
        param1 = NULL;
        param2 = NULL;
        param3 = NULL;
        //printf("%s\n", line);
    }

    // free all memory (or not, lol)
    END:
    //clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    //uint64_t delta_us = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
    //printf("%f ms", (double)delta_us/1000);

    exit(0);
}