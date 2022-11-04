#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <curl/curl.h>

#include "gtfs_req.h"

struct buildbuf{
    uint8_t* response;
    size_t sz, cap;
};

void init_buildbuf(struct buildbuf* bb, int cap){
    bb->response = malloc(cap);
    bb->sz = 0;
    bb->cap = cap;
}

/*this must be written to a uint8_t* buf*/
size_t curl_writefunc(void* buf, size_t size, size_t nmemb, void* vmem){
    size_t bytes = size*nmemb;
    struct buildbuf* mem = vmem;
    uint8_t* tmpbuf;

    if(mem->sz+bytes > mem->cap){
        mem->cap = MAX(mem->cap*2, mem->cap+bytes);
        tmpbuf = malloc(mem->cap);
        memcpy(tmpbuf, mem->response, mem->sz);
        free(mem->response);
        mem->response = tmpbuf;
    }
    memcpy(mem->response+mem->sz, buf, bytes);
    mem->sz += bytes;

    /*printf("recvd %li total bytes, cap: %li\n", mem->sz, mem->cap);*/
    return size*nmemb;
}

enum train traintotrain(char train){
    switch(train){
        case 'A':
        case 'a':
        case 'C':
        case 'c':
        case 'E':
        case 'e':
            return ACE;
        
        case 'B':
        case 'b':
        case 'D':
        case 'd':
        case 'F':
        case 'f':
        case 'M':
        case 'm':
            return BDFM; 
        case 'G':
        case 'g':
            return G;
        
        case 'J':
        case 'j':
        case 'Z':
        case 'z':
            return JZ;
        case 'N':
        case 'n':
        case 'Q':
        case 'q':
        case 'R':
        case 'r':
        case 'W':
        case 'w':
            return NQRW;
        case 'L':
        case 'l':
            return L;
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
            return NUMBERS;
    }
    return TRAIN_MAX;
}

struct mta_req* setup_mr(){
    struct mta_req* mr = malloc(sizeof(struct mta_req));
    curl_global_init(CURL_GLOBAL_DEFAULT);
    mr->curl = curl_easy_init();
    curl_easy_setopt(mr->curl, CURLOPT_WRITEFUNCTION, curl_writefunc);
    return mr;
}

void cleanup_mr(struct mta_req* mr){
    curl_slist_free_all(mr->hdr);
    curl_easy_cleanup(mr->curl);
    curl_global_cleanup();
    free(mr);
}

void prep_curl(CURL* curl, char* header, struct buildbuf* bb){
    struct curl_slist* headers = NULL;

    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)bb);
    headers = curl_slist_append(headers, header);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
}

uint8_t* curl_request(CURL* curl, char* url, int* len, CURLcode* res){
    struct buildbuf bb;
    CURLcode status;

    curl_easy_setopt(curl, CURLOPT_URL, url);

    init_buildbuf(&bb, 0);
    // TODO: header shouldn't be repeatedly updated - it stays the same
    prep_curl(curl, "x-api-key: 3Mna5AMSgo15Cd41NJ61OaeqgzjezcMb4HxCQH5J", &bb);

    status = curl_easy_perform(curl);
    if(res)*res = status;
    *len = bb.sz;

    return bb.response;
}

uint8_t* mta_request(struct mta_req* mr, enum train line, int* len, CURLcode* res){
    char url[100] = "https://api-endpoint.mta.info/Dataservice/mtagtfsfeeds/nyct%2Fgtfs";

    memcpy(url+66, url_lookup[line], url_lookup_len[line]);
    return curl_request(mr->curl, url, len, res);
}
