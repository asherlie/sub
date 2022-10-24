#include "dir.h"

/*
 * S30S
 * ignore leading letter[s]
 * trailing N == +1, S == +2
*/
struct stop{
    char* stop_id;
    char* stop_name;
    enum direction dir;

    struct stop* next;
};


struct stopmap{
    int n_buckets;
    struct stop** buckets;
};

int stop_id_hash(char* stop_id, int n_buckets, enum direction* dir);
void init_stopmap(struct stopmap* sm, int n_buckets);
void build_stopmap(struct stopmap* sm, FILE* fp_in);
void free_stopmap(struct stopmap* sm);
char* lookup_stopmap(struct stopmap* sm, char* stop_id, enum direction* dir);
