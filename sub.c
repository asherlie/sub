#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

    data = mta_request(mr, NUMBERS, &len, &res);
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
