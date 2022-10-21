#include <stdio.h>

#include "gtfs-realtime.pb-c.h"
#if !1

// carriage_* refers to car numbers
/* TransitRealtime__VehiclePosition methods */
void   transit_realtime__vehicle_position__init
                     (TransitRealtime__VehiclePosition         *message);

size_t transit_realtime__vehicle_position__get_packed_size
                     (const TransitRealtime__VehiclePosition   *message);
size_t transit_realtime__vehicle_position__pack
                     (const TransitRealtime__VehiclePosition   *message,
                      uint8_t             *out);
size_t transit_realtime__vehicle_position__pack_to_buffer
                     (const TransitRealtime__VehiclePosition   *message,
                      ProtobufCBuffer     *buffer);
TransitRealtime__VehiclePosition *
       transit_realtime__vehicle_position__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);

void   transit_realtime__vehicle_position__free_unpacked
                     (TransitRealtime__VehiclePosition *message,
                      ProtobufCAllocator *allocator);
/* TransitRealtime__Alert methods */

void   transit_realtime__trip_update__init
                     (TransitRealtime__TripUpdate         *message);
size_t transit_realtime__trip_update__get_packed_size
                     (const TransitRealtime__TripUpdate   *message);
size_t transit_realtime__trip_update__pack
                     (const TransitRealtime__TripUpdate   *message,
                      uint8_t             *out);
size_t transit_realtime__trip_update__pack_to_buffer
                     (const TransitRealtime__TripUpdate   *message,
                      ProtobufCBuffer     *buffer);
TransitRealtime__TripUpdate *
       transit_realtime__trip_update__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   transit_realtime__trip_update__free_unpacked
                     (TransitRealtime__TripUpdate *message,
                      ProtobufCAllocator *allocator);


#endif
int main(){
/*
 *     struct TransitRealtime__VehiclePosition vpos;
 * 
 *     transit_realtime__vehicle_position__init(&vpos);
 *     transit_realtime__vehicle_position_un
*/
    TransitRealtime__TripUpdate tu; // - hmm, this contains stu
    TransitRealtime__TripUpdate__StopTimeUpdate stu;

    transit_realtime__trip_update__init(&tu);
    /*
     * okay, i should be using struct  TransitRealtime__TripUpdate__StopTimeUpdate - i pass it a stop_id and somehow set a GTFS feed
     * maybe using TransitRealtime__FeedMessage
    */
    transit_realtime__trip_update__stop_time_update__init(&stu);
    stu.stop_id = "613N";
    printf("%s\n", stu.stop_id);
    // ayyy, i think i just have to grab the bytes from an http request and unpack them using *_unpack(), which will populate a struct!
}
