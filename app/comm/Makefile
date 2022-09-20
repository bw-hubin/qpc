# Include the nanopb provided Makefile rules
include ../nanopb/extra/nanopb.mk

# Compiler flags to enable all warnings & debug info
CFLAGS = -ansi -Wall -Werror -g -O0 -std=gnu99
CFLAGS += -I$(NANOPB_DIR)

all: comm.pb.c comm.pb.h

.SUFFIXES:

clean:
	rm -f server comm.pb.c comm.pb.h

server: server.c common.c comm.pb.c server.c 
	$(CC) $(CFLAGS) -o $@ $^ $(NANOPB_CORE)

