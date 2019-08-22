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

#define DOUBLE_HASHING_FACTOR 1

#define MAX_ENTITIES_NUMBER 100000
#define MAX_RELATIONSHIPS_NUMBER 100000

#define DA_RESIZE_THRESHOLD_PERCENTAGE 98
#define DA_GROWTH_FACTOR 2
#define INITIAL_DA_SIZE 100

static const int dummy = 1;
static unsigned long int id = 0;

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

static unsigned long long inline calcul_hash(const void *buffer) {
    return djb2(buffer);
}

struct ht_item {
    char *key;
    unsigned long long int hash;
    void *value;
};

int compare_ht_items(const void *a, const void *b) {
    const struct ht_item *pa = *(const struct ht_item **) a;
    const struct ht_item *pb = *(const struct ht_item **) b;

    return strcmp(pa->key, pb->key);
}

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
    if (double_hashing_round > 0) {
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
    size_t non_null_count;
    size_t size;
};

struct din_arr *din_arr_new(size_t initial_size) {
    struct din_arr *arr = malloc(sizeof(struct din_arr));
    if (arr == NULL) {
        exit(666);
    }
    arr->array = calloc(initial_size, sizeof(void *));
    if (arr->array == NULL) {
        exit(666);
    }
    arr->size = initial_size;
    arr->next_free = 0;
    arr->non_null_count = 0;
    return arr;
}

size_t din_arr_resize(struct din_arr *arr, size_t new_size) {
    if (new_size <= arr->size) {
        return arr->size;
    }
    arr->array = realloc(arr->array, sizeof(void *) * new_size);
    if (arr->array == NULL) {
        exit(666);
    }
    arr->size = new_size;
    return new_size;
}

void din_arr_insert(struct din_arr *arr, unsigned long int index, void *elem, size_t elem_size) {
    if (index >= arr->size) {
        size_t old_size = arr->size;
        size_t new_size = din_arr_resize(arr, index + 1);
        if (new_size <= old_size) {
            exit(666);
        }
    }
    if (elem != NULL) {
        if (arr->array[index] != NULL) {
            free(arr->array[index]);
        } else {
            arr->non_null_count++;
        }
    } else {
        arr->non_null_count--;
        free(arr->array[index]);
    }
    arr->array[index] = malloc(elem_size);
    memcpy(arr->array[index], elem, elem_size);
    arr->next_free = index >= arr->next_free ? index + 1 : arr->next_free;
}

void din_arr_append(struct din_arr *arr, void *elem, size_t elem_size) {
    din_arr_insert(arr, arr->next_free, elem, elem_size);
}

void din_arr_remove(struct din_arr *arr, void *elem, int (*cmp)(void *, void *)) {
    for (size_t i = 0; i < arr->next_free; i++) {
        if (cmp(arr->array[i], elem) == 0) {
            free(arr->array[i]);
            arr->array[i] = NULL;
        }
    }
}

void din_arr_sort(struct din_arr *arr, int (*cmp)(const void *a, const void *b)) {
    qsort(arr->array, arr->next_free, sizeof(void *), cmp);
}

void din_arr_zero(struct din_arr *arr) {
    for (size_t i = 0; i < arr->next_free; i++) {
        if (arr->array[i] != NULL) {
            free(arr->array[i]);
            arr->array[i] = NULL;
        }
    }
    arr->non_null_count = 0;
    arr->next_free = 0;
}

struct din_arr *din_arr_copy(struct din_arr *arr) {
    struct din_arr *new = malloc(sizeof(struct din_arr *));
    if (new == NULL) {
        exit(666);
    }
    new->array = malloc(arr->size * sizeof(void *));
    if (new->array == NULL) {
        exit(666);
    }
    memcpy(new->array, arr->array, arr->size * sizeof(void *));
    new->size = arr->size;
    new->next_free = arr->next_free;
    new->non_null_count = arr->non_null_count;

    return new;
}

void din_arr_destroy(struct din_arr *arr) {
    size_t i;
    for (i = 0; i < arr->size; i++) {
        free(arr->array[i]);
    }
    free(arr->array);
    free(arr);
}

void din_arr_soft_destroy(struct din_arr *arr) {
    size_t i;
    free(arr->array);
    free(arr);
}

void din_arr_print(struct din_arr *arr) {
    printf("\n[");
    for (size_t i = 0; i < arr->next_free; i++) {
        printf("%s, ", arr->array[i]);
    }
    printf("]\n");
}

struct int_arr {
    unsigned long int *array;
    unsigned long int next_free;
    size_t non_zero_count;
    size_t size;
};

struct int_arr *int_arr_new(size_t initial_size) {
    struct int_arr *arr = malloc(sizeof(struct int_arr));
    if (arr == NULL) {
        exit(666);
    }
    arr->array = calloc(initial_size, sizeof(unsigned long int));
    if (arr->array == NULL) {
        exit(666);
    }
    arr->next_free = 0;
    arr->non_zero_count = 0;
    arr->size = initial_size;
    return arr;
}

struct int_arr *int_arr_copy(struct int_arr *arr) {
    if (arr->size == 0) {
        printf("ALLARMEEE");
        exit(666);
    }
    struct int_arr *new = malloc(sizeof(struct int_arr));
    if (new == NULL) {
        exit(666);
    }
    new->array = malloc(arr->size * sizeof(unsigned long int));
    if (new->array == NULL) {
        exit(666);
    }
    memcpy(new->array, arr->array, arr->size * sizeof(unsigned long int));
    new->next_free = arr->next_free;
    new->non_zero_count = arr->non_zero_count;
    return new;
}

size_t int_arr_resize(struct int_arr *arr, size_t new_size) {
    if (new_size <= arr->size) {
        return arr->size;
    }
    arr->array = realloc(arr->array, sizeof(unsigned long int) * new_size);
    if (arr->array == NULL) {
        exit(666);
    }
    arr->size = new_size;
    return new_size;
}

void int_arr_insert(struct int_arr *arr, size_t index, unsigned long int value) {
    if (index >= arr->size) {
        int_arr_resize(arr, index * 2);
    }
    if (value != 0 && arr->array[index] == 0) {
        ++arr->non_zero_count;
    } else if (value == 0 && arr->array[index] != 0) {
        --arr->non_zero_count;
    }
    arr->next_free = index + 1;
    arr->array[index] = value;
}

void int_arr_append(struct int_arr *arr, unsigned long int value) {
    int_arr_insert(arr, arr->next_free, value);
}

void int_arr_remove(struct int_arr *arr, unsigned long int value) {
    if (value == 0) {
        return;
    }
    for (size_t i = 0; i < arr->next_free; i++) {
        if (arr->array[i] == value) {
            arr->array[i] = 0;
        }
    }
}

void int_arr_print(struct int_arr *arr) {
    printf("\n[");
    for (size_t i = 0; i < arr->next_free; i++) {
        printf("%llu, ", arr->array[i]);
    }
    printf("]\n");
}

void int_arr_destroy(struct int_arr *arr) {
    free(arr->array);
    free(arr);
}

int cmp_ints(const void *a, const void *b) {
    return *(unsigned long int *) a - *(unsigned long int *) b;
}

void int_arr_sort(struct int_arr *arr) {
    qsort(arr->array, arr->next_free, sizeof(unsigned long int), cmp_ints);
}

struct holder {
    struct int_arr *in;
    struct int_arr *out;
};

struct holder *holder_new() {
    struct holder *hold = malloc(sizeof(struct holder));
    if (hold == NULL) {
        exit(666);
    }
    hold->in = int_arr_new(10);
    hold->out = int_arr_new(10);
    return hold;
}

void add_ent(struct hash_table *mon_ent_ids, struct din_arr *mon_ent_ids_inverse, char *entity_name) {
    if (ht_get(mon_ent_ids, entity_name) == NULL) {
        ht_insert(mon_ent_ids, entity_name, (void *) ++id);
        din_arr_insert(mon_ent_ids_inverse, id, entity_name, (strlen(entity_name) + 1) * sizeof(char));
    }
}

void add_rel(struct hash_table *mon_ent_ids, struct hash_table *mon_rel, struct hash_table *rel_present_check,
             char *origin_ent, char *dest_ent, char *rel_name) {
    unsigned long int origin_id = (unsigned long int) ht_get(mon_ent_ids, origin_ent);
    unsigned long int dest_id = (unsigned long int) ht_get(mon_ent_ids, dest_ent);
    char *key;
    if (origin_id == 0 || dest_id == 0) {
        return;
    }
    size_t len_orig = strlen(origin_ent);
    size_t len_dest = strlen(dest_ent);

    key = malloc(sizeof(char) * (len_orig + len_dest + 2));
    strcpy(key, origin_ent);
    key[len_orig] = '-';
    key[len_orig + 1] = '\0';
    strcat(key, dest_ent);

    struct hash_table *rel_table = ht_get(rel_present_check, rel_name);
    if (rel_table != NULL) {
        if (ht_get(rel_table, key) != NULL) {
            return;
        }
    } else {
        rel_table = ht_new(INITIAL_HASH_TABLE_SIZE);
        ht_insert(rel_present_check, rel_name, rel_table);
    }

    struct holder *rel_hold = ht_get(mon_rel, rel_name);
    if (rel_hold == NULL) {
        rel_hold = holder_new();
        ht_insert(mon_rel, rel_name, rel_hold);
    }
    int_arr_append(rel_hold->in, dest_id);
    int_arr_append(rel_hold->out, origin_id);
    ht_insert(rel_table, key, &dummy);
}

void del_ent(struct hash_table *mon_ent_ids, struct din_arr *mon_ent_ids_reverse, struct hash_table *mon_rel,
             char *entity_name) {
    unsigned long int ent_id = (unsigned long int) ht_get(mon_ent_ids, entity_name);
    if (ent_id == 0) {
        return;
    }
    size_t cnt = 0;
    for (size_t i = 0; i < mon_rel->size && cnt < mon_rel->count; i++) {
        if (mon_rel->array[i] == NULL || mon_rel->array[i] == &HT_DELETED_ITEM) {
            continue;
        }
        cnt++;
        struct holder *rel_hold = mon_rel->array[i]->value;
        for (size_t j = 0; j < rel_hold->in->next_free; j++) {
            if (0 != rel_hold->in->array[j] && rel_hold->in->array[j] == ent_id) {
                int_arr_insert(rel_hold->in, j, 0);
                int_arr_insert(rel_hold->out, j, 0);
            }
        }
        if (rel_hold->in->non_zero_count == 0) {
            free(mon_rel->array[i]->key);
            free(mon_rel->array[i]->value);
            free(mon_rel->array[i]);
            mon_rel->array[i] = &HT_DELETED_ITEM;
        } else {
            for (size_t j = 0; j < rel_hold->out->next_free; j++) {
                if (0 != rel_hold->out->array[j] && rel_hold->out->array[j] == ent_id) {
                    int_arr_insert(rel_hold->in, j, 0);
                    int_arr_insert(rel_hold->out, j, 0);
                }
            }
        }
        if (rel_hold->in->non_zero_count == 0) {
            ht_delete(mon_rel, mon_rel->array[i]->key);
        }
    }

    din_arr_insert(mon_ent_ids_reverse, ent_id, NULL, 0);
    ht_delete(mon_ent_ids, entity_name);
}

void del_rel(struct hash_table *mon_ent_ids, struct hash_table *mon_rel,
             char *origin_ent, char *dest_ent, char *rel_name) {
    struct holder *rel_hold = ht_get(mon_rel, rel_name);
    if (rel_hold == NULL) {
        return;
    }
    unsigned long int origin_id = ht_get(mon_ent_ids, origin_ent);
    unsigned long int dest_id = ht_get(mon_ent_ids, dest_ent);

    if (origin_id == NULL || dest_id == NULL) {
        return;
    }

    for (size_t j = 0; j < rel_hold->in->next_free; j++) {
        if (0 != rel_hold->in->array[j] &&
            rel_hold->in->array[j] == origin_id &&
            0 != rel_hold->out->array[j] &&
            rel_hold->out->array[j] == dest_id) {
            int_arr_insert(rel_hold->in, j, 0);
            int_arr_insert(rel_hold->out, j, 0);
            break;
        }
    }

    if (rel_hold->in->non_zero_count == 0) {
        ht_delete(mon_rel, rel_name);
    }
}

void report(struct hash_table *mon_ent_ids, struct din_arr *mon_ent_ids_reverse, struct hash_table *mon_rel) {

    struct int_arr *maxes = NULL;

    int count = 0;
    unsigned long int current = 0;
    int max = 0;
    if (mon_rel->count == 0) {
        printf("none\n");
        return;
    }

    struct din_arr *mon_rel_list = din_arr_new(mon_rel->count);
    for (size_t i = 0, cnt = 0; i < mon_rel->size && cnt < mon_rel->count; i++){
        if (mon_rel->array[i] == NULL || mon_rel->array[i] == &HT_DELETED_ITEM){
            continue;
        }
        cnt++;
        din_arr_append(mon_rel_list, mon_rel->array[i], sizeof(struct ht_item));
    }
    din_arr_sort(mon_rel_list, compare_ht_items);

    for (size_t i = 0; i < mon_rel_list->next_free; i++) {
        struct ht_item *item = mon_rel_list->array[i];
        count = 0;
        current = 0;
        max = 0;
        if (maxes != NULL) {
            int_arr_destroy(maxes);
        }
        maxes = int_arr_new(INITIAL_DA_SIZE);
        //int_arr_print(((struct holder*)mon_rel->array[i]->value)->in);
        struct int_arr *temp_arr = int_arr_copy(((struct holder *) item->value)->in);
        //int_arr_print(temp_arr);
        int_arr_sort(temp_arr);
        //int_arr_print(temp_arr);
        // now temp_arr contains sorted numbers (aka pointers)

        for (size_t j = 0; j < temp_arr->next_free; j++) {
            //printf("%d\n", temp_arr->array[j]);
            if (temp_arr->array[j] == 0) {
                continue;
            }

            if (current == 0) {
                current = temp_arr->array[j];
            }

            if (current == temp_arr->array[j]) {
                ++count;
            }

            if (current != temp_arr->array[j]) {
                if (count == max) {
                    int_arr_append(maxes, current);
                } else if (count > max) {
                    max = count;
                    int_arr_destroy(maxes);
                    maxes = int_arr_new(INITIAL_DA_SIZE);
                    int_arr_append(maxes, current);
                }
                count = 1;
                current = temp_arr->array[j];
            }

            if (temp_arr->next_free == j + 1) {
                if (count == max) {
                    int_arr_append(maxes, current);
                } else if (count > max) {
                    max = count;
                    int_arr_destroy(maxes);
                    maxes = int_arr_new(INITIAL_DA_SIZE);
                    int_arr_append(maxes, current);
                }
                count = 0;
                current = 0;
            }
        }

        printf("\"%s\"", item->key);
        struct din_arr *ents = din_arr_new(maxes->next_free);
        for (size_t j = 0; j < maxes->next_free; j++) {
            char *name = mon_ent_ids_reverse->array[maxes->array[j]];
            din_arr_append(ents, name, sizeof(char) * (strlen(name) + 1));
        }
        din_arr_sort(ents, compare_strings);
        for (size_t j = 0; j < ents->next_free; j++) {
            printf(" \"%s\"", ents->array[j]);
        }
        din_arr_destroy(ents);
        printf(" %d;", max);
        if (i + 1 < mon_rel_list->next_free){
            printf(" ");
        } else {
            printf("\n");
        }
        int_arr_destroy(temp_arr);
        int_arr_destroy(maxes);

    }
    din_arr_destroy(mon_rel_list);

}

int main(void) {
    /*struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);*/
    freopen("input.txt", "r", stdin);
    freopen("output.txt", "w", stdout);
    char line[MAX_LINE_LENGTH];

    struct hash_table *mon_ent_ids, *mon_rel, *rel_present_check;
    struct din_arr *mon_ent_ids_inverse;

    mon_ent_ids = ht_new(INITIAL_MON_ENT_SIZE);
    mon_ent_ids_inverse = din_arr_new(INITIAL_MON_ENT_SIZE);

    mon_rel = ht_new(INITIAL_MON_REL_SIZE);
    rel_present_check = ht_new(INITIAL_MON_REL_SIZE);

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

            if (token[len - 1] == '\n') {
                token[len - 1] = '\0';
                len--;
            }
            if (token[0] == '"' && token[len - 1] == '"') {
                token++;
                token[len - 2] = '\0';
                len -= 2;
            } else if (cur_par > 0) {
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
                    add_ent(mon_ent_ids, mon_ent_ids_inverse, param1);
                }
            } else if (strcmp(action, action_del_ent) == 0) {
                if ((param1 != NULL && param1[0] != '\0') &&
                    (param2 == NULL || param2[0] == '\0')
                    && (param3 == NULL || param3[0] == '\0')) {
                    del_ent(mon_ent_ids, mon_ent_ids_inverse, mon_rel, param1);
                }
            } else if (strcmp(action, action_add_rel) == 0) {
                if ((param1 != NULL && param1[0] != '\0') &&
                    (param2 != NULL && param2[0] != '\0')
                    && (param3 != NULL && param3[0] != '\0')) {
                    add_rel(mon_ent_ids, mon_rel, rel_present_check, param1, param2, param3);
                }
            } else if (strcmp(action, action_del_rel) == 0) {
                if ((param1 != NULL && param1[0] != '\0') &&
                    (param2 != NULL && param2[0] != '\0')
                    && (param3 != NULL && param3[0] != '\0')) {
                    del_rel(mon_ent_ids, mon_rel, param1, param2, param3);
                }
            } else if (strcmp(action, action_report) == 0) {
                if ((param1 == NULL || param1[0] == '\0') &&
                    (param2 == NULL || param2[0] == '\0')
                    && (param3 == NULL || param3[0] == '\0')) {
                    report(mon_ent_ids, mon_ent_ids_inverse, mon_rel);
                }
            } else if (strcmp(action, "end") == 0) {
                goto END;
            }
        }

        memset(line, 0, MAX_LINE_LENGTH);
        action = NULL;
        param1 = NULL;
        param2 = NULL;
        param3 = NULL;
    }

    // free all memory (or not, lol)
    END:
    /*clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    uint64_t delta_us = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
    printf("%f ms", (double)delta_us/1000);*/

    exit(0);
}

int xmain() {
    char lez[10] = "ciao";
    printf("%llu", calcul_hash(lez));
}