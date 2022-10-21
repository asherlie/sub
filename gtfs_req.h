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

const char* url_lookup[] = {"-ace", "-bdfm", "-g", "-jz", "-nqrw", "-l", ""};
const int url_lookup_len[] = {4, 5, 2, 3, 5, 2, 0};

struct mta_req{
    struct curl_slist* hdr;
    CURL* curl;
};

struct mta_req* setup_mr();
void cleanup_mr(struct mta_req* mr);
uint8_t* curl_request(CURL* curl, char* url, int* len, CURLcode* res);

uint8_t* mta_request(struct mta_req* mr, enum train line, int* len);
