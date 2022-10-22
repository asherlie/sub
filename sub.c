#include <stdio.h>
#include <stdlib.h>

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

int main(){
/*
 *     struct TransitRealtime__VehiclePosition vpos;
 * 
 *     transit_realtime__vehicle_position__init(&vpos);
 *     transit_realtime__vehicle_position_un
*/
    struct mta_req* mr = setup_mr();
    uint8_t* data;
    int len;
    CURLcode res;
    ProtobufCAllocator allocator = {.alloc=pb_malloc, .free=pb_free, .allocator_data=NULL};

    TransitRealtime__FeedMessage* feedmsg;

    (void)url_lookup;

    data = mta_request(mr, NUMBERS, &len, &res);
    printf("read %i bytes, result: %i\n", len, res);

    feedmsg = transit_realtime__feed_message__unpack(&allocator, len, data);
    #if !1
    feedmsg->n_entity;
    feedmsg->entity->trip_update[0];
    feedmsg->entity->trip_update[0]->n_stop_time_update;
    feedmsg->entity->trip_update[0]->stop_time_update[0]->departure->time;
    feedmsg->entity->trip_update[0]->trip->base;
    #endif
    if(feedmsg){
        for(size_t i = 0; i < feedmsg->n_entity; ++i){
            /*printf("vehicle label: %s\n", feedmsg->entity[i]->vehicle->vehicle->label);*/
            for(size_t j = 0; j < feedmsg->entity[i]->trip_update->n_stop_time_update; ++j){
                struct TransitRealtime__TripUpdate__StopTimeUpdate* stu = feedmsg->entity[i]->trip_update->stop_time_update[j];
/*printf("label: %s\n", feedmsg->entity[i]->trip_update->stop_time_update[j]->*/
                time_t t = stu->arrival->time;
                printf("arriving to stop_id: %s at: %s\n", stu->stop_id, ctime(&t));
            }
        }
    }
}
