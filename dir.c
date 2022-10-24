#include "dir.h"

const char* dirtostr(enum direction d){
    switch(d){
        case NORTH:
            return "north";
        case SOUTH:
            return "south";
        default:
            break;
    }
    return "unspecified";
}
