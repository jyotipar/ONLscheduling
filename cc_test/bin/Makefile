INCLUDES = -I../lib
LIBS = -L ../lib -lglpk -lmappers
CFLAGS = -D_GNU_SOURCE -D_REENTRANT -D_THREAD_SAFE -W -Wall -Wundef -Wimplicit -Wno-deprecated -Woverloaded-virtual
#DEBUG = -g -pg
DEBUG = -g
CC     = g++
AR     = /usr/bin/ar
RANLIB = /usr/bin/ranlib

SRC1 = request.cc stats.cc
OBJ1 = $(patsubst %.cc,%.o,$(SRC1))
CMD1 = request

SRC2 = gen_random_topology.cc
OBJ2 = $(patsubst %.cc,%.o,$(SRC2))
CMD2 = gen_random_topology

SRC3 = gen_random_topology2.cc
OBJ3 = $(patsubst %.cc,%.o,$(SRC3))
CMD3 = gen_random_topology2

SRC4 = gen_start_times.cc
OBJ4 = $(patsubst %.cc,%.o,$(SRC4))
CMD4 = gen_start_times

all: $(CMD1) $(CMD2) $(CMD3) $(CMD4)

$(OBJ1) : %.o : %.cc
	${CC} ${DEBUG} ${CFLAGS} ${INCLUDES} -o $@ -c $<

$(CMD1) : % : ${OBJ1}
	$(CC) ${DEBUG} -o $@ ${OBJ1} $(LIBS)

$(OBJ2) : %.o : %.cc
	${CC} ${DEBUG} ${CFLAGS} ${INCLUDES} -o $@ -c $<

$(CMD2) : % : ${OBJ2}
	$(CC) ${DEBUG} -o $@ ${OBJ2} $(LIBS)

$(OBJ3) : %.o : %.cc
	${CC} ${DEBUG} ${CFLAGS} ${INCLUDES} -o $@ -c $<

$(CMD3) : % : ${OBJ3}
	$(CC) ${DEBUG} -o $@ ${OBJ3} $(LIBS)

$(OBJ4) : %.o : %.cc
	${CC} ${DEBUG} ${CFLAGS} ${INCLUDES} -o $@ -c $<

$(CMD4) : % : ${OBJ4}
	$(CC) ${DEBUG} -o $@ ${OBJ4} $(LIBS)

clean:
	/bin/rm -f $(CMD1) $(OBJ1) $(CMD2) $(OBJ2) $(CMD3) $(OBJ3) $(CMD4) $(OBJ4)
