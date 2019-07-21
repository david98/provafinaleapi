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
    printf("count: %ld size: %ld\n", ht->count, ht->size);
    if (ht->count == ht->size){
       ht_resize(ht, ht->size * 2);
    } else if (ht->count <= (ht->size/4)){
        ht_resize(ht, ht->size / 2);
    }

    long int index = ht_get_index(ht, item->key, 0);
    printf("hash table index: %ld\n", index);

    int i = 1;
    // TODO: fix infinite loop in insertion
    while (ht->array[index] != NULL && strcmp(item->key, ht->array[index]->key) != 0) {
        index = ht_get_index(ht, item->key, i);
        printf("index: %ld\n", index);
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
    list->head = malloc(sizeof(struct list));
    list->tail = list->head;
    list->head->prev = NULL;
    list->head->prev = NULL;
    list->head->elem = NULL;
    list->length = 0;
}

void list_append(struct list *list, void *elem){
    list->tail->elem = elem;
    list->tail->next = malloc(sizeof(struct list_node));
    list->tail->next->elem = NULL;
    list->tail->next->prev = list->tail;
    list->tail->next->next = NULL;
    list->tail = list->tail->next;
}

void list_print(struct list *list){
    struct list_node *cur = list->head;
    while( cur != NULL ){
        printf("%s ", cur->elem);
        cur = cur->next;
    }
}

void add_ent(char *entity_name, const int *dummy, struct hash_table *mon_ent, struct list *mon_ent_list){
    if (ht_get(mon_ent, entity_name) == NULL) {
        printf("hash\n");
        ht_insert(mon_ent, entity_name, dummy);
        printf("list\n");
        list_append(mon_ent_list, entity_name);
    }
}

int main() {

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

    const int dummy = 1;

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

        printf("%s %s %s %s\n", action, param1, param2, param3);

        if (strcmp(action, action_add_ent) == 0){
            add_ent(param1, &dummy, &mon_ent, &mon_ent_list);
        } else if (strcmp(action, action_del_ent) == 0){
            // del ent
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

    return 0;
}