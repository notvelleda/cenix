CC=cc
CFLAGS=-I. -I../include -std=c89
LDFLAGS=

BINARY=ld
OBJECTS=ld.o

all: ${BINARY}

%.o: %.c
	${CC} ${CFLAGS} -c $<

${BINARY}: ${OBJECTS}
	${CC} ${LDFLAGS} ${OBJECTS} -o ${BINARY}

clean:
	-rm ${BINARY} ${OBJECTS}
