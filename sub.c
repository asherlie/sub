#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdatomic.h>

#include "sub.h"
#include "dir.h"

#include "stopmap.h"
#include "gtfs_req.h"
#include "gtfs-realtime.pb-c.h"

void* pb_malloc(void* alloc_data, size_t size){
    (void)alloc_data;
    return malloc(size);
}

void pb_free(void* alloc_data, void* ptr){
    (void)alloc_data;
    free(ptr);
}

enum direction train_direction(char* stop_id){
    switch(stop_id[strlen(stop_id)-1]){
        case 'N':
        case 'n':
            return NORTH;
        case 'S':
        case 's':
            return SOUTH;
    }
    return NONE;
}

void init_train_arrivals(struct train_arrivals* ta, int n_buckets){
    ta->n_buckets = n_buckets;
    ta->train_stop_buckets = calloc(n_buckets, sizeof(struct train_stop*));
    ta->bucket_locks = calloc(sizeof(pthread_mutex_t*), ta->n_buckets);
    ta->next_lock = malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(ta->next_lock, NULL);
    memset(ta->pop_map, 0, TRAIN_MAX);
}

// do we need to lock in every circumstance - what if !create_missing - we do! because other threads might be inserting
// even if we're only reading
struct train_stop* lookup_train_stop_internal(struct train_arrivals* ta, char* lat, char* lon, _Bool create_missing, pthread_mutex_t** bucket_lock){
/*
 *     we'll be using full train name as a hash - unless there's a better id - station id maybe
 *     stop id is stop and train line
 *     aha! use lat long
 *     obviously LOL
 *     
 *     sum of every single digit - pretty random will have good spread
 *     can check if we found our exact stop by using lat long
 * 
 *     or we can check using strcmp() if we'd like
*/

    /*int stop_idx = stop_id_hash(stop_id, ta->n_buckets, NULL);*/
    // can use a lock for each stop_idx, acquire it here 
    int stop_idx = lat_lon_hash(lat, lon, ta->n_buckets, LAT_LON_PRECISION);
    struct train_stop* ts, * prev = NULL;
    pthread_mutex_t* nul_lock = NULL, * tmp_lock;
    /*if(atomic_compare_exchange_strong(ta->bucket_locks+stop_idx, (pthread_mutex_t**)NULL, ta->next_lock)){*/
    /* this may not exactly be threadsafe - what if next_lock is allocated but not initialized
     */
    if(atomic_compare_exchange_strong(ta->bucket_locks+stop_idx, &nul_lock, ta->next_lock)){
        /*printf("INITIALIZED NEW LOCK FOR IDX: %i\n", stop_idx);*/
        tmp_lock = malloc(sizeof(pthread_mutex_t));
        pthread_mutex_init(tmp_lock, NULL);
        /* in the worst case, next_lock can be re-used
         * this would occur when two NUL locks are found before either thread gets a chance
         * to update ta->next_lock
         */
        atomic_store(&ta->next_lock, tmp_lock);
    }
    *bucket_lock = ta->bucket_locks[stop_idx];
    pthread_mutex_lock(*bucket_lock);
    /**bucket_lock = */
    // we can do a CAS to set our new mutex if not exist
    // we can do this by always keeping a pre-initialized mutex attached to ta
    // member: pthread_mutex_t* next_mutex
    // then if CAS call is a success, we can just initialize a new mutex
    // pthread_mutex_lock
    ts = ta->train_stop_buckets[stop_idx];
    for(; ts; ts = ts->next){
        /*if(!memcmp(ts->lat, lat, 9) && !memcmp(ts->lon, lon, 10))return ts;*/
        /*ts->lat is only 2 bytes after the period - latloncmp is returning equality when it shouldn't */
        if(latloncmp(ts->lat, lat, LAT_LON_PRECISION) && latloncmp(ts->lon, lon, LAT_LON_PRECISION))return ts;
        prev = ts;
    }
    if(!create_missing)return NULL;
    /* ts == NULL at this point */
    ts = malloc(sizeof(struct train_stop));
    ts->next = NULL;
    ts->arrivals = NULL;
    /*ts->stop_id = strdup(stop_id);*/
    memcpy(ts->lat, lat, LAT_LON_PRECISION);
    memcpy(ts->lon, lon, LAT_LON_PRECISION+1);
    if(!prev){
        ta->train_stop_buckets[stop_idx] = ts;
    }
    else{
        prev->next = ts;
    }
    return ts;
}

/* arrival lists should possibly be separated based on direction
 * we also need to be able to efficiently update existing arrivals
 * if we can binary search to find 
 */
// returns n updated arrival times
// TODO: without a unique identifier for each stop, which i believe does exist but that we don't have access to service_id
// nvm we do actually - it's in struct  TransitRealtime__TripUpdate__TripProperties.trip_id
// TODO: use this is time_t scheduled_arrival doesn't work
// even though it's a string it can be converted to an int reliably
// because this is only relevant for this applicatin once we know the station and train already - we just need 
// an identifier to know which scheduled arrival this is for updating
// we can probably use just the number at the end of trip_id
// must be threadsafe - if this is, we'll be good to parallely populate()
// there will be a lock for each lat lon bucket, initialized when we insert a new lat lon bucket
// lookup_train_stop_internal() will set a lock ptr that we release at the end of this function
struct train_arrival* insert_arrival_time(struct train_arrivals* ta, char* lat, char* lon, char* train, enum direction dir, time_t time, time_t departure, int32_t delay){
    // is this threadsafe? nope - this is where insertion occurs
    pthread_mutex_t* lock;
    struct train_stop* ts = lookup_train_stop_internal(ta, lat, lon, 1, &lock);
    /*int updates = 0;*/
    /* is there a unique identifier i can compare? need to commpare specific arrivals to make sure they're the same
     * in case we're updating an existing one
            i can use stoptimeupdate.optional uint32 stop_sequence = 1;
     */
    /* TODO: optionally just insert into the back - i'll need a pointer to last 
     * this can be done if we know we're not updating and doing an initial insertion
     */
    _Bool prepend = 0;
    struct train_arrival* arr, * tmp_arr, * prev_arr = NULL;
    if(!ts->arrivals || time >= ts->arrivals->arrival){
        for(arr = ts->arrivals; arr; arr = arr->next){
            if(arr->arrival+arr->delay == time+delay && arr->dir == dir){
            /*
             * we need to insert in prev once we've found our ins point
             * we can break once we've reached > time
            */
            /*if(arr->stop_sequence_id == stop_sequence_id){*/
                // TODO: do this atomically
                arr->arrival = time;
                arr->departure = departure;
                arr->delay = delay;
                /*puts("updated existing");*/
                pthread_mutex_unlock(lock);
                return arr;
            }
            /* we know to insert between prev and arr */
            if(arr->arrival > time)break;
            prev_arr = arr;
        }
    }
    /* if this should be idx 0 */
    else prepend = 1;
    tmp_arr = malloc(sizeof(struct train_arrival));
    tmp_arr->dir = dir;
    tmp_arr->train = train;
    /*arr->stop_sequence_id = stop_sequence_id;*/
    tmp_arr->arrival = time;
    tmp_arr->departure = departure;
    tmp_arr->delay = delay;
    tmp_arr->next = NULL;

    // TODO: link in new arrival atomically
    if(prepend){
        prev_arr = ts->arrivals;
        tmp_arr->next = prev_arr;
        ts->arrivals = tmp_arr;
        goto RET;
    }
    if(!prev_arr){
        ts->arrivals = tmp_arr;
    }
    else{
        // 
        prev_arr->next = tmp_arr;
        tmp_arr->next = arr;
    }
    RET:
    pthread_mutex_unlock(lock);
    return tmp_arr;
}

struct train_stop* lookup_train_stop(struct train_arrivals* ta, char* lat, char* lon){
     struct train_stop* ret;
     pthread_mutex_t* lock;
     ret = lookup_train_stop_internal(ta, lat, lon, 0, &lock);
     pthread_mutex_unlock(lock);
     return ret;
}
/*should print 'arrived _ minutes ago, departing in _' if in past*/
// TODO: should the indexing function ignore N/S? that way we can look at all arrivals in one place
// train_names should be replaced with a map - an array of size n_ascii that is set to indicate which train lines to query
// map['c'] = 1, map['2'] = 1 will be c train and 2 train
// TODO: depending on which are triggered, we can selectively populate our struct
struct train_arrival** lookup_upcoming_arrivals(struct train_stop* ts, _Bool* train_lines, time_t cur_time, enum direction dir, int n_arrivals){
    struct train_arrival** arrivals = calloc(sizeof(struct train_arrival), n_arrivals);
    int secs_before_departure = 0;
    int idx = 0;
    for(struct train_arrival* ta = ts->arrivals; ta; ta = ta->next){
        /* TODO: dir is redundant, same info is captured in stop name - 101N, for ex. */
        // TODO URGENT: we should check if train has departed, not arrived. we can then print trains that are currently at station as well
        /*if(ta->arrival > cur_time-secs_before_departure && (dir == NONE || ta->dir == dir) && (!train_lines || train_lines[(int)*ta->train]))arrivals[idx++] = ta;*/
        /* append already-arrived trains if they haven't yet departed */
        /*arrival > now - (seconds in the past)*/
        /*if(ta->arrival > ((ta->departure) ? ta->departure : cur_time-secs_before_departure) && */
        if((ta->departure ? ta->departure : ta->arrival+secs_before_departure) > cur_time &&
        /*if(ta->arrival > cur_time-secs_before_departure && */
                                                                                               /* this occurs with shuttles */
           (dir == NONE || ta->dir == dir) && (!train_lines || train_lines[(int)*ta->train] || (ta->train[1] && train_lines[(int)ta->train[1]]))){
               /*printf("departure: %li\n", ta->departure);*/
               arrivals[idx++] = ta;
           }
    }
    return arrivals;
}

void conv_time(time_t arrival, int* mins, int* secs){
    *mins = arrival/60;
    *secs = arrival-(*mins*60);
}

// must be threadsafe - lock free!
// or individual locks for each _
/* TODO: because insert_arrival_time() is threadsafe, we can make this multithreaded even within a single feemdsg */
int feedmsg_to_train_arrivals(const struct TransitRealtime__FeedMessage* feedmsg, struct train_arrivals* ta, struct stopmap* stop_id_map){
    char* train_name = NULL;
    struct stop* s;
    int insertions = 0;
    /* need to use n threads per feedmsg 
     * for j in n_stop_time_updates:
     *  use thread j%n_threads 
     *
     *  could write a threadpool, use existing pool
     *  or
     *  make changes to the calling function - populate_train_arrivals()
     *  i'll create a new function called concurrent_feedmsg_to_train_arrivals()
     *  this is called by populate_train_arrivals()
     *  it will spawn n_threads
     *  and pass each thread a const pointer to the same feedmsg
     *  each thread will be passed an index to start on
     *  each thread will count by n_threads with logic as follows:
     *      for(int i = start_idx; i < feedmsg_n_entries; i += n_threads)
     *          insert_arrival_time()
     *  this logic applies only to internal indices reflected by `j` below
     */
    for(size_t i = 0; i < feedmsg->n_entity; ++i){
        TransitRealtime__FeedEntity* e = feedmsg->entity[i];
        if(e->vehicle)train_name = e->vehicle->trip->route_id;
        if(!e->trip_update)continue;
        for(size_t j = 0; j < e->trip_update->n_stop_time_update; ++j){
            struct TransitRealtime__TripUpdate__StopTimeUpdate* stu;
            time_t t;

            stu = feedmsg->entity[i]->trip_update->stop_time_update[j];
            if(!stu->arrival)continue;

            t = stu->arrival->time;
            /* why would train_name == NULL? */
            /*might need to look up lat, lon in a map indexed by stop_id*/
            // okay, we have stop_id as provided by stu - we need lat/lon
            // for now i'll be writing a new hashmap that correlates stop_ids with lat/lon
            // should be able to lookup a stop's lat/lon
            // this lookup table will be unchanging and should be written to a file and loaded on startup
            // or even hardcoded programmatically into a c file
            //
            // write another program that appends it to a c file
            //
            // this could even just take in a stop* that it hashes, nay same shite
            /*
             * insert_arrival_time(ta, stu->stop_id, train_name ? strdup(train_name) : "mystery",
             *                     train_direction(stu->stop_id), t, stu->departure ? stu->departure->time : 0, stu->arrival->delay);
            */
            s = lookup_stop_lat_lon(stop_id_map, stu->stop_id);
            if(!s);
            // only this needs to be threadsafe - insert_arrival_time()
            else insert_arrival_time(ta, s->lat, s->lon, train_name ? strdup(train_name) : "mystery",
                                train_direction(stu->stop_id), t, stu->departure ? stu->departure->time : 0,
                                stu->arrival->delay);
            ++insertions;
        }
    }
    /*printf("inserted %i entries  ");*/
    return insertions;
}

struct f2ta_arg{
    const struct TransitRealtime__FeedMessage* feedmsg;
    struct train_arrivals* ta;
    struct stopmap* stop_id_map;
    int n_threads;

    _Atomic int n_insertions;
};

struct f2ta_threadinfo_arg{
    struct f2ta_arg* f2ta_arg;
    int thread_idx;
};

void* feedmsg_to_train_arrivals_th(void* v_f2ta_threadinfo){
    struct f2ta_threadinfo_arg* f2ta_threadinfo = v_f2ta_threadinfo;
    struct f2ta_arg* f2ta_arg = f2ta_threadinfo->f2ta_arg;
    char* train_name = NULL;
    struct stop* s;
    struct TransitRealtime__FeedEntity* e;
    struct TransitRealtime__TripUpdate__StopTimeUpdate* stu;
    time_t t;

    for(size_t entities = 0; entities < f2ta_arg->feedmsg->n_entity; ++entities){
        e = f2ta_arg->feedmsg->entity[entities];
        if(e->vehicle)train_name = e->vehicle->trip->route_id;
        if(!e->trip_update)continue;
        for(size_t update_idx = 0; update_idx < e->trip_update->n_stop_time_update; ++update_idx){
            stu = e->trip_update->stop_time_update[update_idx];
            if(!stu->arrival)continue;

            t = stu->arrival->time;
            s = lookup_stop_lat_lon(f2ta_arg->stop_id_map, stu->stop_id);
            if(s){
                insert_arrival_time(f2ta_arg->ta, s->lat, s->lon, train_name ? strdup(train_name) : "mystery",
                                    train_direction(stu->stop_id), t, stu->departure ? stu->departure->time : 0,
                                    stu->arrival->delay);
                atomic_fetch_add(&f2ta_arg->n_insertions, 1);
            }
        }
    }
    return NULL;
}

// TODO: ensure that this returns the same n_insertions as traditional
int concurrent_feedmsg_to_train_arrivals(const struct TransitRealtime__FeedMessage* feedmsg, struct train_arrivals* ta, struct stopmap* stop_id_map, int n_threads){
    /*struct f2ta_arg* f2ta_args = malloc(sizeof(struct f2ta_arg)*n_threads);*/
    struct f2ta_threadinfo_arg f2ta_threadinfo[n_threads];
    struct f2ta_arg f2ta_arg;
    pthread_t threads[n_threads];

    f2ta_arg.feedmsg = feedmsg;
    f2ta_arg.ta = ta;
    f2ta_arg.stop_id_map = stop_id_map;
    f2ta_arg.n_threads = n_threads;
    f2ta_arg.n_insertions = 0;

    for(int i = 0; i < n_threads; ++i){
        f2ta_threadinfo[i].f2ta_arg = &f2ta_arg;
        f2ta_threadinfo[i].thread_idx = i;
        pthread_create(threads+i, NULL, feedmsg_to_train_arrivals_th, f2ta_threadinfo+i);
    }

    // TODO: should we join threads here?
    
    for(int i = 0; i < n_threads; ++i){
        pthread_join(threads[i], NULL);
    }
    return f2ta_arg.n_insertions;
}

int populate_train_arrivals(struct train_arrivals* ta, enum train train_line, struct stopmap* stop_id_map){
    /* TODO: free mem */
    struct mta_req* mr = setup_mr();
    TransitRealtime__FeedMessage* fm;
    int len;
    CURLcode res;
    /* TODO: free */
                                 // train_line is nonce
    uint8_t* data = mta_request(mr, train_line, &len, &res);
    ProtobufCAllocator allocator = {.alloc=pb_malloc, .free=pb_free, .allocator_data=NULL};

    if(!(fm = transit_realtime__feed_message__unpack(&allocator, len, data)))return 0;
    ta->pop_map[train_line] = 1;
    /*we can also likely use multiple threads to populate from a single feedmsg
     * def can actually, with minimal changes!
     * just need to 
     */
    return concurrent_feedmsg_to_train_arrivals(fm, ta, stop_id_map, 1);
    /*return feedmsg_to_train_arrivals(fm, ta, stop_id_map);*/
}

struct pta_arg{
    struct train_arrivals* ta;
    enum train train_line;
    struct stopmap* stop_id_map;
    int* n_entries;
};

void* populate_train_arrivals_th(void* v_pta_arg){
    struct pta_arg* pta_arg = v_pta_arg;
    int n_entries;
    n_entries = populate_train_arrivals(pta_arg->ta, pta_arg->train_line, pta_arg->stop_id_map);
    if(pta_arg->n_entries)*pta_arg->n_entries = n_entries;
    free(pta_arg);
    return NULL;
}

pthread_t concurrent_populate_train_arrivals(struct train_arrivals* ta, enum train train_line, struct stopmap* stop_id_map){
    pthread_t ret;
    /*this is getting corrupted, stop_id_map is invalidated somehow*/
    struct pta_arg* pta_arg = malloc(sizeof(struct pta_arg));
    pta_arg->ta = ta;
    pta_arg->train_line = train_line;
    pta_arg->stop_id_map = stop_id_map;
    pta_arg->n_entries = NULL;
    pthread_create(&ret, NULL, populate_train_arrivals_th, pta_arg);
    return ret;
}
 
/* TODO: combine all train lines in one struct train_arrivals
 * TODO: move all code to a separate file
 *          once code is separated i'll be able to easily add diff lines to the same struct
 * TODO: add example to continuously update arrival of a given train or trains at stations
 * <> n_upcoming lat lon train_name
 * <> n_upcoming lat lon direction train_name
 */
int main(int a, char** b){

    /*printf("llhash: %i\n", lat_lon_hash("40.884667", "-73.90087", 40));*/

    struct stopmap stop_id_map, lat_lon_map;
    FILE* fp;
    time_t cur_time;

    enum direction dir = (a > 4) ? train_direction(b[4]) : NONE;

    struct train_arrivals ta;
    int n_upcoming;

    if(a < 3)return 1;

    (void)url_lookup;

    fp = fopen("mta_txt/stops.txt", "r");
    init_stopmap(&stop_id_map, 1200);
    init_stopmap(&lat_lon_map, 1200);
    build_stopmap(&lat_lon_map, &stop_id_map, fp);
    fclose(fp);

    init_train_arrivals(&ta, 1200);

    cur_time = time(NULL);

    struct train_stop* ts;
    struct train_arrival** arrivals;
    enum train train_line;
    /* TODO: need to be able to look up stop name from lat lon */
    char* stop_name = lookup_stop_name(&lat_lon_map, b[2], b[3], NULL);
    _Bool train_lines[200] = {0}, negative, specific_pop = 0;
    _Bool pop_map[TRAIN_MAX] = {0};
    pthread_t threads[TRAIN_MAX] = {0};
    int n_threads = 0;
    int mins, secs;

    if(!stop_name){
        puts("no stop found at lat/lon");
        return 1;
    }

    // TODO: populate ta based on the train_lines map
    // all should be populated concurrently - need to make populate_train_arrivals() threadsafe
    // greatly preferable would be lock free
    for(int i = 5; i < a; ++i){
        train_line = traintotrain(*b[i]);
        /*if(!ta.pop_map[train_line])printf("processed %i arrivals from %c\n", populate_train_arrivals(&ta, train_line, &stop_id_map), *b[i]);*/
        /*if(!ta.pop_map[train_line])populate_train_arrivals(&ta, train_line, &stop_id_map);*/
        pop_map[train_line] = 1;
        specific_pop = 1;
        train_lines[toupper(*b[i])] = 1;
    }
    if(!specific_pop)
        memset(pop_map, 1, TRAIN_MAX);

    /* TODO: selectively populate based on which train lines stop at which station
     * in the event that no train line is specified
     */
    // TODO: if only one populate is called don't initialize locks!
    // TODO: see how much slower initializing locks is
    // although this will be unnecessary if we concurrently insert within each train line population
    for(enum train i = 0; i < TRAIN_MAX; ++i){
        if(pop_map[i]){
            /*printf("populating line %i: \"%s\"\n", i, url_lookup[i]);*/
            threads[n_threads++] = concurrent_populate_train_arrivals(&ta, i, &stop_id_map);    
            /*populate_train_arrivals(&ta, i, &stop_id_map);*/
        }
    }

    for(int i = 0; i < n_threads; ++i){
        pthread_join(threads[i], NULL);
    }

    ts = lookup_train_stop(&ta, b[2], b[3]);

    if(!stop_name)stop_name = b[2];
    
    n_upcoming = atoi(b[1]);

    cur_time = time(NULL);
    printf("%i %s-bound upcoming arrivals to \"%s\":\n", n_upcoming, dirtostr(dir), stop_name);
    if(ts){
        arrivals = lookup_upcoming_arrivals(ts, a > 5 ? train_lines : NULL, cur_time, NONE, n_upcoming);
        for(int i = 0, found = 0; found < n_upcoming; ++i){
            if(!arrivals[i])break;
            if(dir != NONE && dir != arrivals[i]->dir){
                /*++n_upcoming;*/
                /*--i;*/
                continue;
            }
            conv_time(arrivals[i]->arrival-cur_time, &mins, &secs);
            negative = mins < 0 || secs < 0;
            /*delay should be red, early green*/
            printf("  %.2i: %s train arrive%s %.2i:%.2i%s", 
                    ++found, arrivals[i]->train, negative ? "d" : "s in", 
                    abs(mins), abs(secs), negative ? " ago" : "");
            conv_time(arrivals[i]->departure-cur_time, &mins, &secs);
            if(negative)printf(" and departs in %.2i:%.2i", mins, secs);
            puts("");
        }
    }
}
