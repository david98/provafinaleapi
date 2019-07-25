#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE_LENGTH 1000
#define INITIAL_HASH_TABLE_SIZE 256u

#define ACTION_SIZE 6

#define ACTION_ADD_ENT "addent"
#define ACTION_DEL_ENT "delent"
#define ACTION_ADD_REL "addrel"
#define ACTION_DEL_REL "delrel"
#define ACTION_REPORT "report"

#define MAX_PARAMETER_SIZE 100

#define MAX_DOUBLE_HASHING_ROUNDS 150

#define MAX_ENTITIES_NUMBER 10000
#define MAX_RELATIONSHIPS_NUMBER 10000

static const int dummy = 1;

int compare_strings (const void * a, const void * b ) {
    const char *pa = *(const char**)a;
    const char *pb = *(const char**)b;

    return strcmp(pa,pb);
}

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
    unsigned long int djb2_hash;
    void *value;
};

static const struct ht_item HT_DELETED_ITEM = {NULL, 0, NULL};

struct hash_table{
    struct ht_item **array;
    unsigned long int size;
    unsigned long int count;
};

void ht_init(struct hash_table *ht, unsigned long int initial_size){
    ht->array = calloc(initial_size, sizeof(struct ht_item));
    if (ht->array == NULL){
        exit(1);
    }
    ht->size = initial_size;
    ht->count = 0;
}

struct hash_table* ht_new(unsigned long int initial_size){
    struct hash_table *ht = malloc(sizeof(struct hash_table));
    if (ht == NULL){
        exit(1);
    }
    ht_init(ht, initial_size);
    return ht;
}

unsigned long int ht_get_index(struct hash_table *ht, char *key, int double_hashing_round){
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

void ht_resize(struct hash_table *ht, unsigned long int new_size){
    struct ht_item **old_array = ht->array;
    ht->array = calloc(new_size, sizeof(struct ht_item));
    if (ht->array == NULL){
        ht_destroy(ht);
        exit(1);
    } else {
        unsigned long int old_size = ht->size;
        ht->size = new_size;
        unsigned long int old_count = ht->count;
        for (int i = 0; i < old_size; i++){
            if (old_array[i] != NULL && old_array[i] != &HT_DELETED_ITEM){
                ht_insert_no_resize(ht, old_array[i]->key, old_array[i]->value);
                free(old_array[i]->key);
                free(old_array[i]);
            }
        }
        ht->count = old_count;
        free(old_array);
    }
}

struct ht_item* ht_new_item(char *key, void *value){
    struct ht_item *item = malloc(sizeof(struct ht_item));
    if (item == NULL){
        exit(1);
    } else {
        item->key = strdup(key);
        if (item->key == NULL){
            free(item);
            exit(1);
        } else {
            item->value = value;
            item->djb2_hash = djb2(key);
        }
    }
    return item;
}

void __ht_insert(struct hash_table *ht, char *key, void *elem, short int resizing){
    struct ht_item *item = ht_new_item(key, elem);

    if (!resizing) {
        if (ht->count == ht->size - 1) {
            ht_resize(ht, ht->size * 2);
        } else if (ht->count <= (ht->size / 4)) {
            ht_resize(ht, ht->size / 2);
        }
    }

    unsigned long int index = ht_get_index(ht, item->key, 0);

    int i = 1;
    // try MAX_DOUBLE_HASHING_ROUNDS times to find a free spot with double hashing
    while (i <= MAX_DOUBLE_HASHING_ROUNDS && ht->array[index] != NULL &&
           ht->array[index] != &HT_DELETED_ITEM &&
           ht->array[index]->djb2_hash != item->djb2_hash &&
           strcmp(item->key, ht->array[index]->key) != 0) {
        index = ht_get_index(ht, item->key, i);
        i++;
    }

    while (ht->array[index] != NULL &&
           ht->array[index] != &HT_DELETED_ITEM &&
           ht->array[index]->djb2_hash != item->djb2_hash &&
           strcmp(item->key, ht->array[index]->key) != 0){
        /* we failed with double hashing, we switch to a different strategy:
         * we linear probe from index to the end and then from start to end
         * (we are guaranteed to find a free spot because we double table size
         *  whenever there's just one left */
        index++;
        if (index == ht->size){
            index = 0;
        }
    }
    if (ht->array[index] != NULL && ht->array[index] != &HT_DELETED_ITEM){
        free(ht->array[index]->key);
        free(ht->array[index]);
    } else {
        ht->count++;
    }
    ht->array[index] = item;
}

void ht_insert_no_resize(struct hash_table *ht, char *key, void *elem){
    __ht_insert(ht, key, elem, 0);
}

void ht_insert(struct hash_table *ht, char *key, void *elem){
    __ht_insert(ht, key, elem, 1);
}

void* ht_get(struct hash_table *ht, char *key){
    unsigned long int index = ht_get_index(ht, key, 0);
    struct ht_item *item = ht->array[index];
    int i = 1;
    unsigned long int hash = djb2(key);
    short int half_probed = 0;

    while (i < MAX_DOUBLE_HASHING_ROUNDS && item != NULL) {
        if (item != &HT_DELETED_ITEM && hash == item->djb2_hash && strcmp(item->key, key) == 0){
            return item->value;
        }

        index = ht_get_index(ht, key, i);
        item = ht->array[index];
        i++;
    }

    /* Same as above, if we failed to find the item we linear probe
     * */
    while ((index < ht->size || !half_probed ) && item != NULL){
        if (index == ht->size){
            index = 0;
            half_probed = 1;
        }
        if (item != &HT_DELETED_ITEM && hash == item->djb2_hash && strcmp(item->key, key) == 0){
            return item->value;
        }
        index++;
        item = ht->array[index];
    }

    return NULL;
}

void ht_delete(struct hash_table *ht, char *key){
    unsigned long int index = ht_get_index(ht, key, 0);
    unsigned long int hash = djb2(key);
    int i = 1;
    // try MAX_DOUBLE_HASHING_ROUNDS times to find a free spot with double hashing
    while (i <= MAX_DOUBLE_HASHING_ROUNDS && ht->array[index] != NULL) {
        if (ht->array[index] != &HT_DELETED_ITEM && ht->array[index]->djb2_hash == hash &&
            strcmp(key, ht->array[index]->key) == 0){

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

    while ((index < ht->size || !half_probed ) && ht->array[index] != NULL){
        if (ht->array[index] != &HT_DELETED_ITEM && ht->array[index]->djb2_hash == hash &&
            strcmp(key, ht->array[index]->key) == 0){

            free(ht->array[index]->key);
            free(ht->array[index]);
            ht->array[index] = &HT_DELETED_ITEM;
            ht->count--;
            return;
        }
        index++;
        if (index == ht->size){
            index = 0;
            half_probed = 1;
        }
    }
}

void ht_destroy(struct hash_table *ht){
    for (int i = 0; i < ht->size; i++){
        if (ht->array[i] != NULL && ht->array[i] != &HT_DELETED_ITEM) {
            free(ht->array[i]->key);
            free(ht->array[i]->value);
            free(ht->array[i]);
        }
    }
    free(ht);
}

struct list_node{
    struct list_node *prev;
    struct list_node *next;
    void *elem;
};

struct list{
    struct list_node *head;
    struct list_node *tail;
    int length;
};

void list_init(struct list *list){
    list->head = NULL;
    list->tail = NULL;
    list->length = 0;
}

struct list_node* list_create_new_node(void *elem, size_t elem_size){
    struct list_node *node = malloc(sizeof(struct list_node));
    if (node == NULL){
        exit(1);
    }
    node->prev = NULL;
    node->next = NULL;
    node->elem = malloc(elem_size);
    if (node->elem == NULL){
        free(node);
        exit(1);
    }
    memcpy(node->elem, elem, elem_size);
    return node;
}

struct list* list_new() {
    struct list *ls = malloc(sizeof(struct list));
    list_init(ls);
    return ls;
}

void list_append(struct list *list, void *elem, size_t elem_size){
    struct list_node *new_node = list_create_new_node(elem, elem_size);
    if (list->tail == NULL){
        list->head = new_node;
        list->tail = new_node;
    } else {
        new_node->prev = list->tail;
        list->tail->next = new_node;
        list->tail = new_node;
    }
    list->length++;
}

void list_remove(struct list *list, void *elem, int (*cmp)(void*, void*)){
    struct list_node *cur = list->head;
    while (cur != NULL){
        if (cur->elem != NULL && strcmp(cur->elem, elem) == 0){
            if (cur == list->head ){
                if (cur == list->tail){
                    list->head = NULL;
                    list->tail = NULL;
                    free(cur->elem);
                    free(cur);
                } else {
                    list->head = cur->next;
                    free(cur->elem);
                    free(cur);
                }
            } else if (cur == list->tail){
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


void list_print(struct list *list){
    struct list_node *cur = list->head;
    printf("[");
    while( cur != NULL ){
        if (cur->next != NULL) {
            printf("'%s', ", (char *) cur->elem);
        } else {
            printf("'%s'", (char *) cur->elem);
        }
        cur = cur->next;
    }
    printf("]\n");
}

void list_destroy(struct list *list){
    struct list_node *cur = list->head;
    while (cur != NULL){
        struct list_node *next = cur->next;
        free(cur);
        cur = next;
    }
    free(list);
}

void add_ent(char *entity_name, struct hash_table *mon_ent, struct list *mon_ent_list){
    if (ht_get(mon_ent, entity_name) == NULL) {
        ht_insert(mon_ent, entity_name, &dummy);
        list_append(mon_ent_list, entity_name, (strlen(entity_name) + 1) * sizeof(char));
    }
}

void add_rel(char *origin_ent, char *dest_ent, char *rel_name, struct hash_table *mon_ent,
        struct hash_table *mon_rel, struct list *mon_rel_list){
    if (ht_get(mon_ent, origin_ent) != NULL && ht_get(mon_ent, dest_ent) != NULL){
        struct hash_table *rel_table = ht_get(mon_rel, rel_name);
        if (rel_table == NULL){
            rel_table = ht_new(INITIAL_HASH_TABLE_SIZE);
            ht_insert(mon_rel, rel_name, rel_table);
            list_append(mon_rel_list, rel_name, sizeof(char) * (strlen(rel_name) + 1));
        }
        struct hash_table *dest_table = ht_get(rel_table, dest_ent);
        if (dest_table == NULL){
            dest_table = ht_new(INITIAL_HASH_TABLE_SIZE);
            ht_insert(rel_table, dest_ent, dest_table);
        }
        if (ht_get(dest_table, origin_ent) == NULL){
            ht_insert(dest_table, origin_ent, &dummy);
        }
    }
}

void del_ent(char *entity_name, struct hash_table *mon_ent, struct list *mon_ent_list){
    if (ht_get(mon_ent, entity_name) != NULL) {
        ht_delete(mon_ent, entity_name);
        list_remove(mon_ent_list, entity_name, strcmp);
    }
}

void del_rel(char *origin_ent, char *dest_ent, char *rel_name,
        struct hash_table *mon_rel, struct list *mon_rel_list){
    struct hash_table *rel_table = ht_get(mon_rel, rel_name);
    if (rel_table != NULL){
        struct hash_table *dest_table = ht_get(rel_table, dest_ent);
        if (dest_table != NULL){
            int *flag = ht_get(dest_table, origin_ent);
            if (flag != NULL){
                ht_delete(dest_table, origin_ent);
                if (dest_table->count == 0){
                    ht_destroy(dest_table);
                    ht_delete(rel_table, dest_ent);
                    if (rel_table->count == 0){
                        ht_destroy(rel_table);
                        ht_delete(mon_rel, rel_name);
                        list_remove(mon_rel_list, rel_name, strcmp);
                    }
                }
            }
        }
    }
}

void report(struct hash_table *mon_ent, struct list *mon_ent_list,
        struct hash_table *mon_rel, struct list *mon_rel_list){
    if (mon_rel_list->length == 0){
        printf("none\n");
    } else {
        char *rels[MAX_RELATIONSHIPS_NUMBER];
        int rels_len = 0;
        short int printed = 0;
        struct list_node *rel = mon_rel_list->head;

        while (rel != NULL){
            rels[rels_len] = rel->elem;
            rels_len++;
            rel = rel->next;
        }

        qsort(rels, rels_len, sizeof(char *), compare_strings);

        for (int j = 0; j < rels_len; j++){
            char *cur_rel = rels[j];
            char *best_ents_arr[MAX_ENTITIES_NUMBER];
            int best_ents_arr_len = 0;
            struct hash_table *best_ents = ht_new(INITIAL_HASH_TABLE_SIZE);
            int count = 0;
            struct hash_table *rel_table = ht_get(mon_rel, cur_rel);
            struct list_node *ent = mon_ent_list->head;
            while(ent != NULL){
                struct hash_table *dest_table = ht_get(rel_table, ent->elem);
                if (dest_table != NULL && ht_get(best_ents, ent->elem) == NULL && dest_table->count >= count){
                    struct list *keys_to_remove = list_new();
                    for (int i = 0; i < dest_table->size; i++){
                        if (dest_table->array[i] != NULL && dest_table->array[i] != &HT_DELETED_ITEM &&
                                ht_get(mon_ent, dest_table->array[i]->key) == NULL){
                            list_append(keys_to_remove, dest_table->array[i]->key,
                                        sizeof(char) * (strlen(dest_table->array[i]->key) + 1));
                        }
                    }
                    struct list_node *key = keys_to_remove->head;
                    while( key != NULL ){
                        ht_delete(dest_table, key->elem);
                        key = key->next;
                    }
                    list_destroy(keys_to_remove);

                    if ((best_ents_arr_len == 0 && dest_table->count > 0) || dest_table->count > count){
                        best_ents_arr[0] = ent->elem;
                        best_ents_arr_len = 1;
                        count = dest_table->count;
                    } else if (dest_table->count == count && count != 0 &&
                        strcmp(best_ents_arr[0], ent->elem) != 0){
                        best_ents_arr[best_ents_arr_len] = ent->elem;
                        best_ents_arr_len++;
                    }
                }
                ent = ent->next;
            }

            // sort best_ents_arr
            qsort(best_ents_arr, best_ents_arr_len, sizeof(char *), compare_strings);
            // if count > 0 print report
            if (count > 0){
                printed = 1;
                printf("\"%s\" ", cur_rel);
                for (int i = 0; i < best_ents_arr_len; i++){
                    printf("\"%s\" ", best_ents_arr[i]);
                }
                printf("%d; ", count);
            }
            // if printed at least one line print newline
            ht_destroy(best_ents);
        }

        if (printed){
            printf("\n");
        }
    }

}

int main() {
    freopen("input.txt", "r", stdin);
    struct hash_table mon_ent, mon_rel;
    struct list mon_ent_list, mon_rel_list;
    char line[MAX_LINE_LENGTH];

    /*mon_ent = ht_new(INITIAL_HASH_TABLE_SIZE);
    mon_rel = ht_new(INITIAL_HASH_TABLE_SIZE);*/
    ht_init(&mon_ent, INITIAL_HASH_TABLE_SIZE);
    ht_init(&mon_rel, INITIAL_HASH_TABLE_SIZE);

    list_init(&mon_ent_list);
    list_init(&mon_rel_list);

    const char *action_add_ent = ACTION_ADD_ENT;
    const char *action_del_ent = ACTION_DEL_ENT;
    const char *action_add_rel = ACTION_ADD_REL;
    const char *action_del_rel = ACTION_DEL_REL;
    const char *action_report = ACTION_REPORT;

    char action[ACTION_SIZE + 1];
    char param1[MAX_PARAMETER_SIZE], param2[MAX_PARAMETER_SIZE], param3[MAX_PARAMETER_SIZE];

    while (fgets(line, MAX_LINE_LENGTH, stdin)){
        // parse action
        strncpy(action, line, ACTION_SIZE);
        action[ACTION_SIZE] = '\0';

        //parse parameters
        int line_len = strlen(line);
        int cur_par = 1;
        for (int i = ACTION_SIZE + 2, j = i; i < line_len; i++){
            if (line[i] == '"' && line[i - 1] != ' '){
                char *dest = cur_par == 1 ? param1 : cur_par == 2 ? param2 : param3;
                strncpy(dest, &line[j], i - j);
                line[i - j + 1] = '\0';
                cur_par++;
                j = i + 3;
            }
        }

        if (strcmp(action, action_add_ent) == 0){
            add_ent(param1, &mon_ent, &mon_ent_list);
        } else if (strcmp(action, action_del_ent) == 0){
            del_ent(param1, &mon_ent, &mon_ent_list);
        } else if (strcmp(action, action_add_rel) == 0){
            add_rel(param1, param2, param3, &mon_ent, &mon_rel, &mon_rel_list);
        } else if (strcmp(action, action_del_rel) == 0){
            del_rel(param1, param2, param3, &mon_rel, &mon_rel_list);
        } else if (strcmp(action, action_report) == 0) {
            report(&mon_ent, &mon_ent_list, &mon_rel, &mon_rel_list);
        }
        // zero-out parameters
        memset(param1, 0, MAX_PARAMETER_SIZE);
        memset(param2, 0, MAX_PARAMETER_SIZE);
        memset(param3, 0, MAX_PARAMETER_SIZE);
    }
    // free all memory
    exit(0);
}

int smain(){
    struct hash_table ht;
    ht_init(&ht, 256);

    ht_insert(&ht, "ciao", "hey");
    ht_delete(&ht, "ciao");
    printf("%s", ht_get(&ht, "ciao"));

    exit(0);
}