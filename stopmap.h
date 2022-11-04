#include "dir.h"

#define LAT_LON_PRECISION 5

/*
 * S30S
 * ignore leading letter[s]
 * trailing N == +1, S == +2
*/
struct stop{
    char lat[11];
    char lon[11];
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
//,,,,40.889248,-73.898583,,,1,
int lat_lon_hash(char* lat, char* lon, int n_buckets, int precision);
void init_stopmap(struct stopmap* sm, int n_buckets);
//void build_stopmap(struct stopmap* sm, FILE* fp_in);
void build_stopmap(struct stopmap* lat_lon_sm, struct stopmap* stop_id_sm, FILE* fp_in);
void free_stopmap(struct stopmap* sm);
char* lookup_stop_name(struct stopmap* sm, char* lat, char* lon, enum direction* dir);
struct stop* lookup_stop_lat_lon(struct stopmap* sm, char* stop_id);
_Bool latloncmp(char* a, char* b, int precision);
