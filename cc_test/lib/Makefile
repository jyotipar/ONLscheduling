CFLAGS = -D_GNU_SOURCE -D_REENTRANT -D_THREAD_SAFE -W -Wall -Wundef -Wimplicit -Wno-deprecated -Woverloaded-virtual
#DEBUG = -g -pg
DEBUG = -g
CC     = g++
AR     = /usr/bin/ar
RANLIB = /usr/bin/ranlib

EXT_INC = mappers.h topology.h reservations.h

SRC = topology.cc reservations.cc
OBJS = $(patsubst %.cc,%.o,$(SRC))
LIB = libmappers.a

all: $(LIB)

$(OBJS) : %.o : %.cc
	${CC} ${DEBUG} ${CFLAGS} ${INCLUDES} -o $@ -c $<

$(LIB) : % : $(OBJS)
	$(AR) rc $@ $(OBJS)
	$(RANLIB) $@

clean:
	/bin/rm -f $(LIB) $(OBJS)
