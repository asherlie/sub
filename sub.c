#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sub.h"

#include "stopmap.h"
#include "gtfs_req.h"
#include "gtfs-realtime.pb-c.h"

void* pb_malloc(void* alloc_data, size_t size){
    (void)alloc_data;
    /*printf("allocing %li bytes, passed: %p\n", size, alloc_data);*/
    return malloc(size);
}

void pb_free(void* alloc_data, void* ptr){
    (void)alloc_data;
    /*printf("freeing %p, passed: %p\n", ptr, alloc_data);*/
    free(ptr);
}

/*
 * unmarked: 0
 * northbound: 1
 * southbound: 2
 */
int train_direction(char* stop_id){
    switch(stop_id[strlen(stop_id)-1]){
        case 'N':
            return 1;
        case 'S':
            return 2;
    }
    return 0;
}

#if !1
could keep a struct populated, or actually a hashmap populated
indexable by stop and then train

map[times_square][1_train][0] == northbound arrivals
map[times_square][1_train][1] == southbound arrivals

user sets how often he wants to have this updated

actually this can just be a sample program for live feed

easier too would be command line tool to check next arrivals for a station
could work with the same struct but it is kind of uneccessary overhead

because i could just go through existing structs until i find a string match

would be good to have an easy struct of results though

it must also be possible to print upcoming arrivals at a given station
n_soonest_arrvals(map[times_square], 5, NORTH)
should print the 5 northbounds trains coming the soonest to times square

#endif

int main(){
    struct mta_req* mr = setup_mr();

    struct stopmap sm;
    FILE* fp;
    time_t cur_time;

    uint8_t* data;
    int len;
    CURLcode res;
    ProtobufCAllocator allocator = {.alloc=pb_malloc, .free=pb_free, .allocator_data=NULL};

    TransitRealtime__FeedMessage* feedmsg;

    (void)url_lookup;

    fp = fopen("mta_txt/stops.txt", "r");
    init_stopmap(&sm, 1200);
    build_stopmap(&sm, fp);
    fclose(fp);

    data = mta_request(mr, L, &len, &res);
    cur_time = time(NULL);

    feedmsg = transit_realtime__feed_message__unpack(&allocator, len, data);
    /*
     * we can print N upcoming trains based on responses, they're sorted by arrival time
    */
    char* train_name = NULL;
    if(feedmsg){
        for(size_t i = 0; i < feedmsg->n_entity; ++i){
            /*printf("vehicle label: %s\n", feedmsg->entity[i]->vehicle->vehicle->label);*/
            TransitRealtime__FeedEntity* e = feedmsg->entity[i];
            if(e->vehicle)train_name = e->vehicle->trip->route_id;
            if(!e->trip_update)continue;
            for(size_t j = 0; j < e->trip_update->n_stop_time_update; ++j){
                struct TransitRealtime__TripUpdate__StopTimeUpdate* stu;
                time_t t;
                /*if(!feedmsg->entity[i]->trip_update)continue;*/
                 stu = feedmsg->entity[i]->trip_update->stop_time_update[j];
/*printf("label: %s\n", feedmsg->entity[i]->trip_update->stop_time_update[j]->*/
                if(!stu->arrival)continue;
                t = stu->arrival->time;
                 /*
                  * printf("%s train arriving to stop: %s at: %s\n", train_name, lookup_stopmap(&sm, stu->stop_id), ctime(&t));
                  * printf("%s train arriving to stop: %s at: %li\n", train_name, lookup_stopmap(&sm, stu->stop_id), t-cur_time);
                 */
                 printf("%sbound %s train arriving at stop: %s in: %li seconds\n",
                       (train_direction(stu->stop_id) == 1) ? "north" : "south",
                       train_name, lookup_stopmap(&sm, stu->stop_id), t-cur_time);
            }
        }
    }
}
