#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>

#include "stopmap.h"
/*
 * sm should be generated and stored on disk to be quickly
 * loaded back
 * 
 * this doesn't need to be threadsafe - it will only be read from during
 * runtime
 * 
 * it is meant to be created either on startup or read from disk on startup
 * never touched during runtime unless corrupted
*/

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

int stop_id_hash(char* stop_id, int n_buckets){
    char* idx_s = stop_id + (_Bool)isalpha(*stop_id);
    char c;
    int idx = 0;
    for(int i = 0; idx_s[i]; ++i){
        c = idx_s[i];
        if(idx_s[i] == 'N')c = '0';
        if(idx_s[i] == 'S')c = '1';
        idx += i ? (c-'0')*pow(10, i) : 1;
    }
    return idx%n_buckets;
}

struct stop* lookup_stopmap_internal(struct stopmap* sm, char* stop_id, _Bool create_missing, _Bool* found){
    /*int idx = atoi(stop_idx);*/
    int idx = stop_id_hash(stop_id, sm->n_buckets);
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
    ret->stop_id = stop_id;
    ret->stop_name = NULL;
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
        s->stop_name = stop_name;
    }
}

char* lookup_stopmap(struct stopmap* sm, char* stop_id){
    _Bool found;
    struct stop* s = lookup_stopmap_internal(sm, stop_id, 0, &found);

    if(!s)return NULL;
    return s->stop_name;
}

/*
 * stop_id,stop_code,stop_name,stop_desc,stop_lat,stop_lon,zone_id,stop_url,location_type,parent_station
 * 101,,Van Cortlandt Park-242 St,,40.889248,-73.898583,,,1,
*/
void build_stopmap(struct stopmap* sm, FILE* fp_in){
    char cursor;
    char stop_id[20];
    char stop_name[50];
    int idx = 0, commas = 0;
    int n_lines = 0;

    while((cursor = fgetc(fp_in)) != EOF){
        switch(cursor){
            case '\n':
                idx = 0;
                commas = 0;
                if(n_lines++ > 0){
                    insert_stopmap(sm, strdup(stop_id), strdup(stop_name));
                }
                memset(stop_id, 0, sizeof(stop_id));
                memset(stop_name, 0, sizeof(stop_name));
                break;
            case ',':
                ++commas;
                idx = 0;
                break;
            default:
                if(!commas)stop_id[idx++] = cursor;
                if(commas == 2)stop_name[idx++] = cursor;
        }
    }
}

 int main(int a, char** b){
     struct stopmap sm;
     FILE* fp = a > 1 ? fopen(b[1], "r") : stdin;

     char* ln = NULL, * res;
     size_t sz;
     int b_read;

     init_stopmap(&sm, 1200);
     build_stopmap(&sm, fp);

     while((b_read = getline(&ln, &sz, stdin)) != -1){
        ln[b_read-1] = 0;
        if(res = lookup_stopmap(&sm, ln)){
            puts(res);
        }
     }

     fclose(fp);
     free_stopmap(&sm);
 }
