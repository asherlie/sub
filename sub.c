#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
            return NORTH;
        case 'S':
            return SOUTH;
    }
    return NONE;
}

void init_train_arrivals(struct train_arrivals* ta, int n_buckets){
    ta->n_buckets = n_buckets;
    ta->train_stop_buckets = calloc(n_buckets, sizeof(struct train_stop*));
}

struct train_stop* lookup_train_stop_internal(struct train_arrivals* ta, char* stop_id, _Bool create_missing){
    int stop_idx = stop_id_hash(stop_id, ta->n_buckets, NULL);
    struct train_stop* ts = ta->train_stop_buckets[stop_idx], * prev = NULL;
    for(; ts; ts = ts->next){
        if(!strcmp(ts->stop_id, stop_id))return ts;
        prev = ts;
    }
    if(!create_missing)return NULL;
    /* ts == NULL at this point */
    ts = malloc(sizeof(struct train_stop));
    ts->next = NULL;
    ts->arrivals = NULL;
    ts->stop_id = strdup(stop_id);
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
struct train_arrival* insert_arrival_time(struct train_arrivals* ta, char* stop_id, char* train, enum direction dir, time_t time, time_t departure, int32_t delay){
    struct train_stop* ts = lookup_train_stop_internal(ta, stop_id, 1);
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
        return tmp_arr;
    }
    if(!prev_arr){
        ts->arrivals = tmp_arr;
    }
    else{
        // 
        prev_arr->next = tmp_arr;
        tmp_arr->next = arr;
    }
    return tmp_arr;
}

struct train_stop* lookup_train_stop(struct train_arrivals* ta, char* stop_name, char* stop_id){
     char* stop_id_final = stop_id;
 
     // TODO: we should look up stop_id
     if(!stop_id_final){
        stop_id_final = stop_name;
     }
     return lookup_train_stop_internal(ta, stop_id_final, 0);
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
           (dir == NONE || ta->dir == dir) && (!train_lines || train_lines[(int)*ta->train])){
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

int feedmsg_to_train_arrivals(struct TransitRealtime__FeedMessage* feedmsg, struct train_arrivals* ta){
    char* train_name = NULL;
    int insertions = 0;
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
            insert_arrival_time(ta, stu->stop_id, train_name ? strdup(train_name) : "mystery",
                                train_direction(stu->stop_id), t, stu->departure ? stu->departure->time : 0, stu->arrival->delay);
            ++insertions;
        }
    }
    return insertions;
}

int populate_train_arrivals(struct train_arrivals* ta, enum train train_line){
    /* TODO: free mem */
    struct mta_req* mr = setup_mr();
    TransitRealtime__FeedMessage* fm;
    int len;
    CURLcode res;
    /* TODO: free */
    uint8_t* data = mta_request(mr, train_line, &len, &res);
    ProtobufCAllocator allocator = {.alloc=pb_malloc, .free=pb_free, .allocator_data=NULL};

    if(!(fm = transit_realtime__feed_message__unpack(&allocator, len, data)))return 0;
    return feedmsg_to_train_arrivals(fm, ta);
}
 
/* TODO: combine all train lines in one struct train_arrivals
 * TODO: move all code to a separate file
 *          once code is separated i'll be able to easily add diff lines to the same struct
 * TODO: add example to continuously update arrival of a given train or trains at stations
 * <> n_upcoming stop_id train_name
 */
int main(int a, char** b){
    struct stopmap sm;
    FILE* fp;
    time_t cur_time;

    enum direction dir;

    struct train_arrivals ta;
    int n_upcoming;

    if(a < 3)return 1;

    (void)url_lookup;

    fp = fopen("mta_txt/stops.txt", "r");
    init_stopmap(&sm, 1200);
    build_stopmap(&sm, fp);
    fclose(fp);

    init_train_arrivals(&ta, 1200);

    /* hmm - almost all of these contain no trip updates */
    populate_train_arrivals(&ta, ACE);
    populate_train_arrivals(&ta, BDFM);
    populate_train_arrivals(&ta, G);
    populate_train_arrivals(&ta, NUMBERS);
    populate_train_arrivals(&ta, JZ);
    populate_train_arrivals(&ta, NQRW);
    populate_train_arrivals(&ta, L);

    cur_time = time(NULL);

    /*
     * we can print N upcoming trains based on responses, they're sorted by arrival time
    */
    struct train_stop* ts = lookup_train_stop(&ta, NULL, b[2]);
    struct train_arrival** arrivals;
    char* stop_name = lookup_stopmap(&sm, b[2], &dir);
    _Bool train_lines[200] = {0}, negative;
    int mins, secs;

    for(int i = 3; i < a; ++i){
        train_lines[(int)*b[i]] = 1;
    }

    if(!stop_name)stop_name = b[2];
    
    n_upcoming = atoi(b[1]);

    cur_time = time(NULL);
    printf("%i %s-bound upcoming arrivals to \"%s\":\n", n_upcoming, dirtostr(dir), stop_name);
    if(ts){
        arrivals = lookup_upcoming_arrivals(ts, a > 3 ? train_lines : NULL, cur_time, NONE, n_upcoming);
        for(int i = 0; i < n_upcoming; ++i){
            if(!arrivals[i])break;
            conv_time(arrivals[i]->arrival-cur_time, &mins, &secs);
            negative = mins < 0 || secs < 0;
            /*delay should be red, early green*/
            printf("  %.2i: %s train arrive%s %.2i:%.2i%s", 
                    i+1, arrivals[i]->train, negative ? "d" : "s in", 
                    abs(mins), abs(secs), negative ? " ago" : "");
            conv_time(arrivals[i]->departure-cur_time, &mins, &secs);
            if(negative)printf(" and departs in %.2i:%.2i", mins, secs);
            puts("");
        }
    }
}
