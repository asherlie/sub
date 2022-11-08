#include <stdint.h>
#include <pthread.h>

#include "dir.h"
#include "gtfs_req.h"

struct train_arrival{
    enum direction dir;
    /* unique identifier for train's stop at a station at a time
     * helpful for when updating existing arrival timestamps that
     * have changed
     */
    /// i believe this must be trip_id, not stop_sequence - trip id is harder to compare but should be doable as well
    /// but we don't have access
    ///
    /// okay we don't, we can just compare scheduled arrival though - add time_t with delay - either positive or negative
    /// if arrival+delay == arrival+delay, same!
    /// we'll need to store delay, which we should be doing anyway
    //char* trip_id; // -- see note above int insert_arrival_time
    char* train;
    time_t arrival, departure;
    int32_t delay;
    struct train_arrival* next;
};

struct train_stop{
    char* stop_id;
    char lat[9];
    char lon[9];
    /* sorted linked list of arrivals */
    /* TODO: should north and southbound arrivals be in separated lists? */
    struct train_arrival* arrivals;
    struct train_stop* next;
};

struct train_arrivals{
    int n_buckets;
    struct train_stop** train_stop_buckets;
    pthread_mutex_t** bucket_locks;
    pthread_mutex_t* next_lock;

    _Bool pop_map[TRAIN_MAX];
};

/*
 * for(struct train_arrival* a = arrivals[times_square][7_train]; a; a = a->next){
 *  if(a->dir == NORTH)
 *  etc.
 * }
 */
