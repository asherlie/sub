#include <stdlib.h>

#include "stopmap.h"
// sm should be generated and stored on disk to be quickly
// loaded back

void init_stopmap(struct stopmap* sm, int n_buckets){
    sm->n_buckets = n_buckets;
    sm->buckets = calloc(sizeof(struct stop*), n_buckets);
}

void free_stopmap(struct stopmap* sm){
    for(int i = 0; i < sm->n_buckets; ++i){
        if(sm->buckets[i])free(sm->buckets[i]);
    }
    free(sm->buckets);
}

void insert_stopmap(struct stopmap* sm, char* stop_id, char* stop_name){
}

char* lookup_stopmap(struct stopmap* sm, char* stop_id){
}

int main(){
    struct stopmap sm;
    init_stopmap(&sm, 1000);
    insert_stopmap(&sm, "J14N", "104 St");
    insert_stopmap(&sm, "103N", "238 St");
    free_stopmap(&sm);
}
