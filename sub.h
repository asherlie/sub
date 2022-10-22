enum direction{NORTH, SOUTH};
/*  */
struct train_arrival{
    enum direction dir;
    struct train_arrival* next;
};

struct train_stop{
    char* stop_name;
    /* sorted linked list of arrivals */
    /* TODO: should north and southbound arrivals be in separated lists? */
    struct train_arrival* arrivals;
    struct train_stop* next;
};

struct train_arrivals{
    int n_buckets;
    struct train_stop** train_stop_buckets;
};

/*
 * for(struct train_arrival* a = arrivals[times_square][7_train]; a; a = a->next){
 *  if(a->dir == NORTH)
 *  etc.
 * }
 */
