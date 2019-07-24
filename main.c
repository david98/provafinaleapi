#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE_LENGTH 1000
#define INITIAL_HASH_TABLE_SIZE 256

#define ACTION_SIZE 6

#define ACTION_ADD_ENT "addent"
#define ACTION_DEL_ENT "delent"
#define ACTION_ADD_REL "addrel"
#define ACTION_DEL_REL "delrel"
#define ACTION_REPORT "report"

#define MAX_PARAMETER_SIZE 100

#define MAX_DOUBLE_HASHING_ROUNDS 150

static const int dummy = 1;

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
    long int djb2_hash;
    void *value;
};

static struct ht_item HT_DELETED_ITEM = {NULL, 0, NULL};

struct hash_table{
    struct ht_item **array;
    long int size;
    long int count;
};

void ht_init(struct hash_table *ht, long int initial_size){
    ht->array = calloc(initial_size, sizeof(struct ht_item));
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

void ht_insert(struct hash_table *ht, char *key, void *elem);

void ht_resize(struct hash_table *ht, long int new_size){
    struct ht_item **old_array = ht->array;
    ht->array = calloc(new_size, sizeof(struct ht_item));
    if (ht->array == NULL){
        exit(1);
    } else {
        long int old_size = ht->size;
        ht->size = new_size;
        for (int i = 0; i < old_size; i++){
            if (old_array[i] != NULL && old_array[i] != &HT_DELETED_ITEM){
                ht_insert(ht, old_array[i]->key, old_array[i]->value);
                free(old_array[i]->key);
            }
            free(old_array[i]);
        }
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

void ht_insert(struct hash_table *ht, char *key, void *elem){
    struct ht_item *item = ht_new_item(key, elem);

    ht->count++;
    if (ht->count == ht->size - 1){
       ht_resize(ht, ht->size * 2);
    } else if (ht->count <= (ht->size/4)){
        ht_resize(ht, ht->size / 2);
    }

    long int index = ht_get_index(ht, item->key, 0);

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
    }
    ht->array[index] = item;
}

void* ht_get(struct hash_table *ht, char *key){
    long int index = ht_get_index(ht, key, 0);
    struct ht_item *item = ht->array[index];
    int i = 1;
    long int hash = djb2(key);
    short int half_probed = 0;

    while (i < MAX_DOUBLE_HASHING_ROUNDS && item != NULL && item != &HT_DELETED_ITEM) {
        if (hash == item->djb2_hash && strcmp(item->key, key) == 0){
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
        if (hash == item->djb2_hash && strcmp(item->key, key) == 0){
            return item->value;
        }
        index++;
        item = ht->array[index];
    }

    return NULL;
}

void ht_delete(struct hash_table *ht, char *key){
    printf("\nDELETING %s\n", key);
    ht_insert(ht, key, &HT_DELETED_ITEM);
    if (ht_get(ht, key) != NULL){
        printf("\nDELETION FAILED\n");
    }
    ht->count -= 2;
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
    while( cur != NULL ){
        printf("%s, ", (char*)cur->elem);
        cur = cur->next;
    }
    printf("\n");
}

void add_ent(char *entity_name, struct hash_table *mon_ent, struct list *mon_ent_list){
    if (ht_get(mon_ent, entity_name) == NULL) {
        ht_insert(mon_ent, entity_name, &dummy);
        list_append(mon_ent_list, entity_name, (strlen(entity_name) + 1) * sizeof(char));
        printf("Inserted %s.\n", entity_name);
        list_print(mon_ent_list);
    }
}

void del_ent(char *entity_name, struct hash_table *mon_ent, struct list *mon_ent_list){
    if (ht_get(mon_ent, entity_name) != NULL) {
        ht_delete(mon_ent, entity_name);
        list_remove(mon_ent_list, entity_name, strcmp);
        printf("Removed %s.\n", entity_name);
        list_print(mon_ent_list);
    }
}

int main() {
    freopen("input.txt", "r", stdin);
    struct hash_table mon_ent, mon_rel;
    struct list mon_ent_list, mon_rel_list;
    char line[MAX_LINE_LENGTH];

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
            // del ent
            del_ent(param1, &mon_ent, &mon_ent_list);
        } else if (strcmp(action, action_add_rel) == 0){
            // add rel
        } else if (strcmp(action, action_del_rel) == 0){
            // del rel
        } else if (strcmp(action, action_report) == 0) {
            // report
        }
        // zero-out parameters
        memset(param1, 0, MAX_PARAMETER_SIZE);
        memset(param2, 0, MAX_PARAMETER_SIZE);
        memset(param3, 0, MAX_PARAMETER_SIZE);
    }
    list_print(&mon_ent_list);
    // free all memory

    exit(0);
}