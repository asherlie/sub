CC=gcc
CFLAGS= -Wall -Wextra -Wpedantic -Werror -g3 -lprotobuf-c -lcurl
pwd=$(pwd)

all: sub gtfs_req

transit:
	git clone https://github.com/google/transit

gtfs-realtime.proto: transit
	cp transit/gtfs-realtime/proto/gtfs-realtime.proto .

gtfs-realtime.pb-c.h: gtfs-realtime.proto
	protoc-c gtfs-realtime.proto --c_out=.

sub: sub.c gtfs_req.o gtfs-realtime.pb-c.c

gtfs_req.o: gtfs_req.c
