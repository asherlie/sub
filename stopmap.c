#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>

#include "stopmap.h"
// sm should be generated and stored on disk to be quickly
// loaded back
//
// this doesn't need to be threadsafe - it will only be read from during
// runtime
//
// it is meant to be created either on startup or read from disk on startup
// never touched during runtime unless corrupted

void init_stopmap(struct stopmap* sm, int n_buckets){
    sm->n_buckets = n_buckets;
    sm->buckets = calloc(sizeof(struct stop*), n_buckets);
}

void free_stopmap(struct stopmap* sm){
    for(int i = 0; i < sm->n_buckets; ++i){
        /* TODO: free each string */
        /*for(struct stop* s = sm->buckets[i]; s; s = s->next)*/
        if(sm->buckets[i])free(sm->buckets[i]);
    }
    free(sm->buckets);
}

int stop_id_hash(char* stop_id){
    char* idx_s = stop_id + isalpha(*stop_id);
    char c;
    int idx = 0;
    for(int i = 0; idx_s[i]; ++i){
        c = idx_s[i];
        if(idx_s[i] == 'N')c = '0';
        if(idx_s[i] == 'S')c = '1';
        idx += (c-'0')*pow(10, i);
        /*idx += (idx_s[i] == 'N' || *idx_s == 'S') ? :*/
        /*ascii*/
    }
    return idx;
}

struct stop* lookup_stopmap_internal(struct stopmap* sm, char* stop_id, _Bool create_missing, _Bool* found){
    /*int idx = atoi(stop_idx);*/
    int idx = stop_id_hash(stop_id);
    struct stop* ret = sm->buckets[idx], * prev = NULL;

    for(; ret; ret = ret->next){
        if(!strcmp(ret->stop_id, stop_id)){
            *found = 1;
            return ret;
        }
        prev = ret;
    }
    *found = 0;
    if(!create_missing)return NULL;
    ret = malloc(sizeof(struct stop));
    ret->next = NULL;
    ret->stop_id = strdup(stop_id);
    ret->stop_name = NULL;
    /*printf("ret: %p, prev: %p\n", ret, prev);*/
    /* no match was found, if(prev), we found a matching
     * bucket and can insert after the last element
     *
     * otherwise, create new bucket
     */
    if(prev){
        prev->next = ret;
    }
    else{
        sm->buckets[idx] = ret;
    }
    return ret;
}

void insert_stopmap(struct stopmap* sm, char* stop_id, char* stop_name){
    _Bool found;
    struct stop* s = lookup_stopmap_internal(sm, stop_id, 1, &found);

    if(!found){
        s->stop_name = strdup(stop_name);
    }
}

char* lookup_stopmap(struct stopmap* sm, char* stop_id){
    _Bool found;
    struct stop* s = lookup_stopmap_internal(sm, stop_id, 0, &found);

    return s->stop_name;
}

int main(){
    struct stopmap sm;
    init_stopmap(&sm, 1000);
    insert_stopmap(&sm, "J14N", "104 St");
    insert_stopmap(&sm, "103N", "238 St");
    free_stopmap(&sm);
}
