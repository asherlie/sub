CC=gcc
CFLAGS= -Wall -Wextra -Wpedantic -Werror -g3 -lprotobuf-c -lcurl
gtzip="http://web.mta.info/developers/data/nyct/subway/google_transit.zip"

all: sub

google_transit.zip:
	wget $(gtzip)

mta_txt: google_transit.zip
	mkdir mta_txt
	unzip google_transit.zip -d mta_txt

transit:
	git clone https://github.com/google/transit

gtfs-realtime.proto: transit
	cp transit/gtfs-realtime/proto/gtfs-realtime.proto .

gtfs-realtime.pb-c.c: gtfs-realtime.proto
	protoc-c gtfs-realtime.proto --c_out=.

sub: sub.c gtfs_req.o gtfs-realtime.pb-c.c mta_txt
	$(CC) $(CFLAGS) sub.c gtfs-realtime.pb-c.c gtfs_req.o

gtfs_req.o: gtfs_req.c

.PHONY:
clean:
	rm -rf transit sub gtfs_req.o gtfs-realtime.pb-c.* google_transit.zip mta_txt
