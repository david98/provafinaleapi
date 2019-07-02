#include <stdio.h>
#include <stdlib.h>

#define ENTITY_TYPE 0
#define RELATION_TYPE 1

struct generic_entity {
    char *name;
    short int type; //0: entity, 1: relation
    int id;
};

struct generic_entity** add_entity(struct generic_entity **entity_list, int *length, char *entity_name, short int type){
    struct generic_entity *new_entity = malloc(sizeof(struct generic_entity));
    new_entity->id = *length;
    new_entity->name = entity_name;
    new_entity->type = type;

    int new_length = 0;

    if (*length == 0) {
        entity_list = malloc(sizeof(struct generic_entity *));
        new_length = 1;
    } else {
        new_length = *length + 1;
        entity_list = realloc(entity_list, sizeof(struct generic_entity *) * new_length);
    }
    entity_list[*length] = new_entity;

    *length = new_length;
    return entity_list;
}

void print_entities_array(struct generic_entity **entity_list, int length){

    for (int i = 0; i < length; i++){
        printf("ID: %d, NAME: %s, TYPE: %d\n", entity_list[i]->id, entity_list[i]->name, entity_list[i]->type);
    }
}

int main() {

    struct generic_entity **entity_list = NULL, **relations_list = NULL;
    int next_entity_id = 0, next_relation_id = 0;

    entity_list = add_entity(entity_list, &next_entity_id, "prova", ENTITY_TYPE);
    entity_list = add_entity(entity_list, &next_entity_id, "lezzo", ENTITY_TYPE);
    print_entities_array(entity_list, next_entity_id);

    return 0;
}