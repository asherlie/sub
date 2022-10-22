#include <stdint.h>
#include <curl/curl.h>

enum train{
    ACE = 0,
    BDFM,
    G,
    JZ,
    NQRW,
    L,
    NUMBERS
};

struct mta_req{
    struct curl_slist* hdr;
    CURL* curl;
};

static const char* url_lookup[7] = {"-ace", "-bdfm", "-g", "-jz", "-nqrw", "-l", ""};
static const int url_lookup_len[7] = {4, 5, 2, 3, 5, 2, 0};
/*
 * const char* url_lookup[7];
 * const int url_lookup_len[7];
*/

struct mta_req* setup_mr();
void cleanup_mr(struct mta_req* mr);

uint8_t* mta_request(struct mta_req* mr, enum train line, int* len, CURLcode* res);
