CC=gcc
CFLAGS= -Wall -Wextra -Wpedantic -Werror -O3 -lpthread
pwd=$(pwd)

all: sub gtfs_req

transit:
	git clone https://github.com/google/transit

gtfs-realtime.proto: transit
	cp transit/gtfs-realtime/proto/gtfs-realtime.proto .

gtfs-realtime.pb-c.h: gtfs-realtime.proto
	protoc-c gtfs-realtime.proto --c_out=.

sub: sub.c gtfs-realtime.pb-c.h
	$(CC) $(CFLAGS) sub.c gtfs-realtime.pb-c.c -o sub -lprotobuf-c

gtfs_req: gtfs_req.c
	$(CC) $(CFLAGS) gtfs_req.c -lcurl -o gtfs_req 
