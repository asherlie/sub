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

    /*printf("recvd %i total bytes, cap: %i\n", mem->sz, mem->cap);*/
    return size*nmemb;
}

/*CURL* setup_curl(){*/
struct mta_req* setup_mr(){
    struct mta_req* mr = malloc(sizeof(struct mta_req));
    /*curl_global_init(CURL_GLOBAL_ALL);*/
    curl_global_init(CURL_GLOBAL_DEFAULT);
    mr->curl = curl_easy_init();
    return mr;
}

void cleanup_mr(struct mta_req* mr){
    curl_slist_free_all(mr->hdr);
    curl_easy_cleanup(mr->curl);
    curl_global_cleanup();
    free(mr);
}

void prep_curl(CURL* curl, char* header, struct buildbuf* bb){
    /* TODO: this should be freed using curl_slist_free_all(headers); */
    struct curl_slist* headers = NULL;

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_writefunc);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)bb);
    headers = curl_slist_append(headers, header);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
}

uint8_t* curl_request(CURL* curl, char* url, int* len, CURLcode* res){
    struct buildbuf bb;

    curl_easy_setopt(curl, CURLOPT_URL, url);

    init_buildbuf(&bb, 0);
    prep_curl(curl, "x-api-key: 3Mna5AMSgo15Cd41NJ61OaeqgzjezcMb4HxCQH5J", &bb);

    if(res)*res = curl_easy_perform(curl);
    *len = bb.sz;

    return bb.response;
}

uint8_t* mta_request(struct mta_req* mr, enum train line, int* len){
    char url[100] = "https://api-endpoint.mta.info/Dataservice/mtagtfsfeeds/nyct%2Fgtfs";

    memcpy(url+65, url_lookup[line], url_lookup_len[line]);
    return curl_request(mr->curl, url, len, NULL);
}

// how do i do multiple curl_request calls? need to refresh bb, keep everythign else the same
// actually, should be able to update train we're using too, so url
int main(){
    struct mta_req* mr = setup_mr();

    int len;
    CURLcode res;

    /*
     * for(int i = 0; i < 10; ++i){
     *     free(curl_request(curl, url, &len, &res));
     *     printf("res: %i, read %i bytes\n", res, len);
     * }
    */

    cleanup_mr(mr);
}
