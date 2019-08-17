#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <time.h>

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

#define INITIAL_DM_ROWS 32
#define INITIAL_DM_COLS 32

static const int dummy = 1;
static unsigned long long int next_free_id = 0;

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

char *itoa(unsigned long long int num) {
    char *str = malloc(sizeof(char) * 50);
    if (str == NULL) {
        exit(666);
    }
    sprintf(str, "%llu", num);
    return str;
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
            item->djb2_hash = djb2(key);
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
           ht->array[index]->djb2_hash != item->djb2_hash &&
           strcmp(item->key, ht->array[index]->key) != 0) {
        index = ht_get_index(ht, item->key, i);
        i++;
    }

    int j = 1;
    while (ht->array[index] != NULL &&
           ht->array[index] != &HT_DELETED_ITEM &&
           ht->array[index]->djb2_hash != item->djb2_hash &&
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
    int j = 1;
    while ((index < ht->size || !half_probed) && item != NULL) {
        if (item != &HT_DELETED_ITEM && hash == item->djb2_hash && strcmp(item->key, key) == 0) {
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
            return 1;
        }
        index = ht_get_index(ht, key, i);
        i++;
    }

    short int half_probed = 0;
    int j = 1;
    while ((index < ht->size || !half_probed) && ht->array[index] != NULL) {
        if (ht->array[index] != &HT_DELETED_ITEM && ht->array[index]->djb2_hash == hash &&
            strcmp(key, ht->array[index]->key) == 0) {
            free(ht->array[index]->value);
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

void din_arr_append(struct din_arr *arr, void *elem, size_t elem_size) {
    if (arr->next_free >= arr->size * DA_RESIZE_THRESHOLD_PERCENTAGE / 100) {
        size_t old_size = arr->size;
        size_t new_size = din_arr_resize(arr, arr->size * DA_GROWTH_FACTOR);
        if (new_size <= old_size) {
            exit(666);
        }
    }
    if (elem != NULL) {
        arr->array[arr->next_free] = malloc(elem_size);
        memcpy(arr->array[arr->next_free], elem, elem_size);
        arr->next_free++;
        arr->non_null_count++;
    }
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
        arr->array[index] = malloc(elem_size);
        memcpy(arr->array[arr->next_free], elem, elem_size);
    } else {
        arr->non_null_count--;
        free(arr->array[index]);
        arr->array[index] = elem;
    }
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

void din_arr_destroy(struct din_arr *arr) {
    size_t i;
    for (i = 0; i < arr->size; i++) {
        free(arr->array[i]);
    }
    free(arr->array);
    free(arr);
}

struct din_mat {
    int **rows;
    size_t n_rows;
    size_t n_cols;
    size_t *cols_non_zero_count;
    size_t *rows_non_zero_count;
};

struct din_mat *din_mat_new(size_t initial_num_rows, size_t initial_num_cols) {
    struct din_mat *mat = malloc(sizeof(struct din_mat));
    if (mat == NULL) {
        exit(666);
    }
    mat->rows = malloc(sizeof(int *) * initial_num_rows);
    if (mat->rows == NULL) {
        exit(666);
    }
    for (size_t i = 0; i < initial_num_rows; i++) {
        mat->rows[i] = calloc(initial_num_cols, sizeof(int));
        if (mat->rows[i] == NULL) {
            exit(666);
        }
    }
    mat->n_rows = initial_num_rows;
    mat->n_cols = initial_num_cols;
    mat->rows_non_zero_count = calloc(initial_num_rows, sizeof(size_t));
    mat->cols_non_zero_count = calloc(initial_num_cols, sizeof(size_t));
    if (mat->rows_non_zero_count == NULL || mat->cols_non_zero_count == NULL) {
        exit(666);
    }
    return mat;
}

void din_mat_resize(struct din_mat *mat, size_t new_rows, size_t new_cols) {
    mat->rows = realloc(mat->rows, sizeof(int *) * new_rows);
    if (mat->rows == NULL) {
        exit(666);
    }
    for (size_t i = 0; i < new_rows; i++) {
        mat->rows[i] = realloc(mat->rows[i], sizeof(int) * new_cols);
        size_t j = i < mat->n_rows ? mat->n_cols : 0;
        for (; j < new_cols; j++) {
            mat->rows[i][j] = 0;
        }
        if (mat->rows[i] == NULL) {
            exit(666);
        }
    }
    mat->cols_non_zero_count = realloc(mat->cols_non_zero_count, sizeof(size_t) * new_cols);
    mat->rows_non_zero_count = realloc(mat->rows_non_zero_count, sizeof(size_t) * new_rows);
    if (mat->cols_non_zero_count == NULL || mat->rows_non_zero_count == NULL){
        exit(666);
    }
    for (size_t i = mat->n_cols; i < new_cols; i++){
        mat->cols_non_zero_count[i] = 0;
    }
    for (size_t i = mat->n_rows; i < new_rows; i++){
        mat->rows_non_zero_count[i] = 0;
    }
    mat->n_rows = new_rows;
    mat->n_cols = new_cols;
}

int din_mat_set(struct din_mat *mat, unsigned long int row, unsigned long int col, int val) {
    size_t new_rows = mat->n_rows;
    size_t new_cols = mat->n_cols;
    if (row >= mat->n_rows) {
        new_rows = row + 1;
    }
    if (col >= mat->n_cols) {
        new_cols = col + 1;
    }
    if (new_rows >= mat->n_rows || new_cols >= mat->n_cols) {
        din_mat_resize(mat, new_rows, new_cols);
    }
    if (val != 0 && mat->rows[row][col] == 0) {
        mat->cols_non_zero_count[col]++;
        mat->rows_non_zero_count[row]++;
    } else if (val == 0 && mat->rows[row][col] != 0) {
        mat->cols_non_zero_count[col]--;
        mat->rows_non_zero_count[row]--;
    }
    mat->rows[row][col] = val;
}

int din_mat_get(struct din_mat *mat, unsigned long int row, unsigned long int col) {
    if (row < mat->n_rows && col < mat->n_cols) {
        return mat->rows[row][col];
    } else {
        return 0;
    }
}

void din_mat_set_column(struct din_mat *mat, unsigned long int col, int val) {
    if (col >= mat->n_cols) {
        din_mat_resize(mat, mat->n_rows, col + 1);
    }
    size_t old_col_non_zero_count = mat->cols_non_zero_count[col];
    for (size_t i = 0, count = 0; i < mat->n_rows && count < old_col_non_zero_count; i++) {
        if (din_mat_get(mat, i, col) != 0) {
            count++;
        }
        din_mat_set(mat, i, col, val);
    }
}

void din_mat_set_row(struct din_mat *mat, unsigned long int row, int val) {
    if (row >= mat->n_rows) {
        din_mat_resize(mat, row + 1, mat->n_cols);
    }
    size_t old_row_non_zero_count = mat->rows_non_zero_count[row];
    for (size_t i = 0, count = 0; i < mat->n_rows && count < old_row_non_zero_count; i++) {
        if (din_mat_get(mat, row, i) != 0) {
            count++;
        }
        din_mat_set(mat, row, i, val);
    }
}

void din_mat_print(struct din_mat *mat) {
    for (size_t i = 0; i < mat->n_rows; i++) {
        for (size_t j = 0; j < mat->n_cols; j++) {
            printf("%d ", mat->rows[i][j]);
        }
        printf("\n");
    }
}

void add_ent(char *entity_name, struct hash_table *mon_ent_ids,
             struct hash_table *mon_ent_ids_inverse) {
    /*
     * Only add entity_name to monitored entities if
     * it's not already monitored
     * */
    if (ht_get(mon_ent_ids, entity_name) == NULL) {
        /*
         * Allocate a variable to store entity id
         * */
        unsigned long long int *id = malloc(sizeof(unsigned long long int));
        if (id == NULL) {
            exit(666);
        }
        /*
         * Set id and increment global next free
         * */
        *id = next_free_id;
        next_free_id++;
        /*
         * Generate a string that represents id to be used
         * for reverse lookup
         * */
        char *key = itoa(*id);
        /*
         * Insert mapping from entity_name to its id
         * */
        ht_insert(mon_ent_ids, entity_name, id);
        /*
         * Insert reverse mapping
         * */
        ht_insert(mon_ent_ids_inverse, key, strdup(entity_name));
        free(key);
    }
}

void add_rel(char *origin_ent, char *dest_ent, char *rel_name, struct hash_table *mon_ent_ids,
             struct hash_table *mon_rel, struct din_arr *mon_rel_list) {
    /*
     * Retrieve origin_ent id and dest_ent id
     * */
    unsigned long long int *origin_id = ht_get(mon_ent_ids, origin_ent);
    unsigned long long int *dest_id = ht_get(mon_ent_ids, dest_ent);
    /*
     * If both entities were being monitored
     * */
    if (origin_id != NULL && dest_id != NULL) {
        /*
         * Retrieve matrix for rel_name
         * */
        struct din_mat *rel_mat = ht_get(mon_rel, rel_name);
        /*
         * If rel_name was not being monitored, create
         * a new matrix and append rel_name to mon_rel_list
         * */
        if (rel_mat == NULL) {
            rel_mat = din_mat_new(INITIAL_DM_ROWS, INITIAL_DM_COLS);
            ht_insert(mon_rel, rel_name, rel_mat);
            din_arr_append(mon_rel_list, rel_name,
                           (strlen(rel_name) + 1) * sizeof(char));
        }
        /*
         * Set cell (origin_id, dest_id) to 1
         * */
        din_mat_set(rel_mat, *origin_id, *dest_id, 1);
    }
}

void del_rel(char *origin_ent, char *dest_ent, char *rel_name, struct hash_table *mon_ent_ids,
             struct hash_table *mon_rel) {
    unsigned long long int *origin_id = ht_get(mon_ent_ids, origin_ent);
    unsigned long long int *dest_id = ht_get(mon_ent_ids, dest_ent);
    if (origin_id != NULL && dest_id != NULL) {
        struct din_mat *rel_mat = ht_get(mon_rel, rel_name);
        if (rel_mat != NULL) {
            din_mat_set(rel_mat, *origin_id, *dest_id, 0);
        }
    }
}

void del_ent(char *entity_name, struct hash_table *mon_ent_ids, struct hash_table *mon_rel,
             struct din_arr *mon_rel_list, struct hash_table *mon_ent_ids_inverse) {
    unsigned long long int *ent_id = ht_get(mon_ent_ids, entity_name);
    if (ent_id != NULL) {
        for (size_t i = 0; i < mon_rel_list->next_free; i++) {
            if (mon_rel_list->array[i] != NULL) {
                struct din_mat *rel_mat = ht_get(mon_rel, mon_rel_list->array[i]);
                din_mat_set_column(rel_mat, *ent_id, 0);
                din_mat_set_row(rel_mat, *ent_id, 0);
            }
        }
        ht_delete(mon_ent_ids, entity_name);
        char *key = itoa(*ent_id);
        ht_delete(mon_ent_ids_inverse, key);
        free(key);
    }
}

void report(struct hash_table *mon_rel, struct din_arr *mon_rel_list,
            struct hash_table *mon_ent_ids_inverse) {
    din_arr_sort(mon_rel_list, compare_strings);
    short int printed = 0;
    for (size_t i = 0; i < mon_rel_list->size; i++) {
        char *rel_name = mon_rel_list->array[i];
        if (rel_name != NULL) {
            if (printed) {
                printf(" ");
            }
            struct din_mat *rel_mat = ht_get(mon_rel, rel_name);
            if (rel_mat != NULL) {
                size_t max_count = 0;
                struct din_arr *best_ents = din_arr_new(INITIAL_DA_SIZE);
                for (size_t j = 0; j < rel_mat->n_cols; j++) {
                    if (rel_mat->cols_non_zero_count[j] > max_count) {
                        din_arr_zero(best_ents);
                        din_arr_append(best_ents, &j, sizeof(size_t));
                        max_count = rel_mat->cols_non_zero_count[j];
                    } else if (rel_mat->cols_non_zero_count[j] == max_count && max_count > 0) {
                        din_arr_append(best_ents, &j, sizeof(size_t));
                    }
                }
                struct din_arr *best_ents_names = din_arr_new(best_ents->size);
                for (size_t k = 0; k < best_ents->next_free; k++) {
                    char *key = itoa(*(unsigned long long int *) best_ents->array[k]);
                    char *ent_name = ht_get(mon_ent_ids_inverse, key);
                    free(key);
                    din_arr_append(best_ents_names, ent_name,
                                   (strlen(ent_name) + 1) * sizeof(char));
                }
                din_arr_sort(best_ents_names, compare_strings);
                if (max_count > 0) {
                    printed = 1;
                    printf("\"%s\"", rel_name);
                    for (size_t k = 0; k < best_ents_names->next_free; k++) {
                        printf(" \"%s\"", best_ents_names->array[k]);
                    }
                    printf(" %lu;", max_count);
                }
                din_arr_destroy(best_ents);
                din_arr_destroy(best_ents_names);
            }
        }
    }
    if (!printed) {
        printf("none\n");
    } else {
        printf("\n");
    }
}

int main(void) {
    freopen("input.txt", "r", stdin);
    freopen("output.txt", "w", stdout);

    struct hash_table *mon_ent_ids, *mon_rel, *mon_ent_ids_inverse;
    struct din_arr *mon_rel_list;

    mon_ent_ids = ht_new(INITIAL_MON_ENT_SIZE);
    mon_ent_ids_inverse = ht_new(INITIAL_MON_ENT_SIZE);

    mon_rel = ht_new(INITIAL_MON_REL_SIZE);
    mon_rel_list = din_arr_new(INITIAL_MON_REL_SIZE);

    char line[MAX_LINE_LENGTH];
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
                    add_ent(param1, mon_ent_ids, mon_ent_ids_inverse);
                }
            } else if (strcmp(action, action_del_ent) == 0) {
                if ((param1 != NULL && param1[0] != '\0') &&
                    (param2 == NULL || param2[0] == '\0')
                    && (param3 == NULL || param3[0] == '\0')) {
                    del_ent(param1, mon_ent_ids, mon_rel, mon_rel_list, mon_ent_ids_inverse);
                }
            } else if (strcmp(action, action_add_rel) == 0) {
                if ((param1 != NULL && param1[0] != '\0') &&
                    (param2 != NULL && param2[0] != '\0')
                    && (param3 != NULL && param3[0] != '\0')) {
                    add_rel(param1, param2, param3, mon_ent_ids, mon_rel, mon_rel_list);
                }
            } else if (strcmp(action, action_del_rel) == 0) {
                if ((param1 != NULL && param1[0] != '\0') &&
                    (param2 != NULL && param2[0] != '\0')
                    && (param3 != NULL && param3[0] != '\0')) {
                    del_rel(param1, param2, param3, mon_ent_ids, mon_rel);
                }
            } else if (strcmp(action, action_report) == 0) {
                if ((param1 == NULL || param1[0] == '\0') &&
                    (param2 == NULL || param2[0] == '\0')
                    && (param3 == NULL || param3[0] == '\0')) {
                    report(mon_rel, mon_rel_list, mon_ent_ids_inverse);
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

    exit(0);
}

int smain() {
    struct din_mat *mat = din_mat_new(10, 10);
    din_mat_set(mat, 20, 20, 1);
    printf("%lu %lu\n", mat->n_rows, mat->n_cols);
}