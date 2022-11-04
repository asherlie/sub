#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>

#include "dir.h"
#include "stopmap.h"

/*
 * this doesn't need to be threadsafe - it will only be read from during
 * runtime
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

int stop_id_hash(char* stop_id, int n_buckets, enum direction* dir){
    char* idx_s = stop_id + (_Bool)isalpha(*stop_id);
    char c;
    int idx = 0;
    for(int i = 0; idx_s[i]; ++i){
        c = idx_s[i];
        if(idx_s[i] == 'N'){
            c = '0';
            if(dir)*dir = NORTH;
        }
        else if(idx_s[i] == 'S'){
            c = '1';
            if(dir)*dir = SOUTH;
        }
        else if(dir)*dir = NONE;
        idx += i ? (c-'0')*pow(10, i) : 1;
    }
    return idx%n_buckets;
}

int ctoi(char c){
    return (!c || c == '.' || c == '-') ? 0 : c-'0';
}

_Bool latloncmp(char* a, char* b, int precision){
    int to = precision + (*a == '-');
    for(int i = 0; i < to; ++i){
        if(a[i] != b[i])return 0;
    }
    return 1;
}

int lat_lon_hash(char* lat, char* lon, int n_buckets, int precision){
    int idx = 0;
    // assuming strlen(lat) == strlen(lon) == 9
    // the leading negative 
    for(int i = 0; i < precision; ++i)
        idx += ctoi(lat[i]) + ctoi(lon[i]);
    idx += ctoi(lon[precision]);
    return idx % n_buckets;
}

/*
 * we just need to look up lat_lon INDEX/bucket not even exact entry
 * by stop_id - have a map indexed by stop_id
 * stop id should have a 1:1 lookup
 * damn wont work maybe
 * i just need a way to get from stop_id to lat lon
 * can just have a separate map but dang...
*/
/* abstract this to accept idx - we'll use same fn for lat lon and stop_id 
 * acceptable because stop_id will always be set and we'll reuse indices
 * there will be a call to insert_lat_lon
 * and teh return value of that will be inserted into stop_id hash
 * nvm separate bt still abstract
 */
/* if(stop_id) */
struct stop* lookup_stopmap_internal(struct stopmap* sm, char* stop_id, char* lat, char* lon, _Bool create_missing, int lat_lon_precision, _Bool* found){
    /*int idx = atoi(stop_idx);*/
    enum direction dir = NONE;
    /*int idx = stop_id_hash(stop_id, sm->n_buckets, &dir);*/
    int idx = (stop_id) ? stop_id_hash(stop_id, sm->n_buckets, NULL) : lat_lon_hash(lat, lon, sm->n_buckets, lat_lon_precision);
    /*! ret!*/
    struct stop* ret = sm->buckets[idx], * prev = NULL;

    for(; ret; ret = ret->next){
        /*if(!strcmp(ret->stop_id, stop_id)){*/
        /*
         * if(stop_id && !ret->stop_id)puts("SEGGG");
         * fflush(stdout);
        */
        /*if((stop_id && !strcmp(stop_id, ret->stop_id)) || (!stop_id && !memcmp(ret->lat, lat, lat_lon_precision) && !memcmp(ret->lon, lon, lat_lon_precision+1))){*/
        if((stop_id && !strcmp(stop_id, ret->stop_id)) || (!stop_id && latloncmp(ret->lat, lat, lat_lon_precision) && latloncmp(ret->lon, lon, lat_lon_precision))){
            /*
             * this is being reahed when it really shouldn't be
             * latloncmp() is equating lat/lon pairs that it really shouldn't
             * for example:
             *     40.835537, -73.9214 with 40.807722, -73.96411
            */
            *found = 1;
            return ret;
        }
        prev = ret;
    }
    *found = 0;
    if(!create_missing)return NULL;
    ret = malloc(sizeof(struct stop));
    ret->next = NULL;
    /* struct stop contains an unused field stop_id so it can be reused for stop_id -> lat lon mapping */
    ret->stop_id = stop_id;
    /* if inserting lat, lon will never be NULL */
    memcpy(ret->lat, lat, lat_lon_precision);
    memcpy(ret->lon, lon, lat_lon_precision+1);
    ret->lat[lat_lon_precision] = 0;
    ret->lon[lat_lon_precision+1] = 0;
    ret->dir = dir;
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

/*with eaxch insrtion to lat lon hash we can insert the same stop* to our stop id hash*/
/*void insert_stopmap(struct stopmap* sm, char* lat, char* lon, char* stop_name){*/
void insert_lat_lon_map(struct stopmap* sm, char* lat, char* lon, char* stop_name){
    _Bool found;
    struct stop* s = lookup_stopmap_internal(sm, NULL, lat, lon, 1, LAT_LON_PRECISION, &found);

    if(!found){
        s->stop_name = stop_name;
    }
}

void insert_stop_id_map(struct stopmap* sm, char* stop_id, char* lat, char* lon){
    _Bool found;
    lookup_stopmap_internal(sm, stop_id, lat, lon, 1, LAT_LON_PRECISION, &found);
}

char* lookup_stop_name(struct stopmap* sm, char* lat, char* lon, enum direction* dir){
    _Bool found;
    struct stop* s = lookup_stopmap_internal(sm, NULL, lat, lon, 0, LAT_LON_PRECISION, &found);

    if(!s)return NULL;
    if(dir)*dir = s->dir;
    return s->stop_name;
}

struct stop* lookup_stop_lat_lon(struct stopmap* sm, char* stop_id){
    _Bool found;
    return lookup_stopmap_internal(sm, stop_id, NULL, NULL, 0, LAT_LON_PRECISION, &found);
}

/*
 * stop_id,stop_code,stop_name,stop_desc,stop_lat,stop_lon,zone_id,stop_url,location_type,parent_station
 * 101,,Van Cortlandt Park-242 St,,40.889248,-73.898583,,,1,
*/
void build_stopmap(struct stopmap* lat_lon_sm, struct stopmap* stop_id_sm, FILE* fp_in){
    char cursor;
    char stop_id[20] = {0};
    char lat[11] = {0};
    char lon[11] = {0};
    char stop_name[50] = {0};
    int idx = 0, commas = 0;
    int n_lines = 0;

    while((cursor = fgetc(fp_in)) != EOF){
        switch(cursor){
            case '\n':
                idx = 0;
                commas = 0;
                if(n_lines++ > 0){
                    /*printf("lat: \"%s\", lon: \"%s\"\n", lat, lon);*/
                    // if identical lat/lon we'll need to overwrite
                    /* TODO: we can share ptrs to lat, lon if careful about freeing
                     * if not freeing then shouldn't make a diff
                     */
                    /*
                     * if(!strcmp("L28S", stop_id)){
                     *     puts("it's being inserted");
                     * }
                    */
                    /*printf("inserting %s, %s, %s, \"%s\"\n", lat, lon, stop_id, stop_name);*/
                    if(lat_lon_sm)insert_lat_lon_map(lat_lon_sm, strdup(lat), strdup(lon), strdup(stop_name));
                    if(stop_id_sm)insert_stop_id_map(stop_id_sm, strdup(stop_id), strdup(lat), strdup(lon));
                }
                memset(stop_id, 0, sizeof(stop_id));
                memset(stop_name, 0, sizeof(stop_name));
                memset(lat, 0, sizeof(lat));
                memset(lon, 0, sizeof(lon));
                break;
            case ',':
                ++commas; 
                idx = 0;
                /*
                 * if(commas == 5){
                 * -- its happening on commas == 5, idx 9 == 3 -- we shouldn't insert the NEGATIVE! or we should and it needs an extra byte
                 * the last digit of longitude isn't being added anywhere and could be corrupting adjacent memory! could be the probby
                 *     puts("DB");
                 * }
                */
                break;
            default:
                // stop_id is correct until we get to crit section, when it's corrupted
                if(!commas)stop_id[idx++] = cursor;
                if(commas == 4)
                    lat[idx++] = cursor;
                else if(commas == 5)
                    lon[idx++] = cursor;
                else if(commas == 2)stop_name[idx++] = cursor;
        }
    }
}

#if 0
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
#endif
