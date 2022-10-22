/*
 * S30S
 * ignore leading letter[s]
 * trailing N == +1, S == +2
*/
struct stop{
    char* stop_id;
    char* stop_name;

    struct stop* next;
};

struct stop_lst{
};

struct stopmap{
    struct stop** buckets;
};

void init_stopmap(struct stopmap* sm, int n_buckets);
