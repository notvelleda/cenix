CC=cc
CFLAGS=-I. -I../include -std=c89
LDFLAGS=

BINARY=cc
OBJECTS=lexer.o main.o parser.o hashtable.o ir.o codegen.o

all: ${BINARY}

%.o: %.c
	${CC} ${CFLAGS} -c $<

${BINARY}: ${OBJECTS}
	${CC} ${LDFLAGS} ${OBJECTS} -o ${BINARY}

clean:
	-rm ${BINARY} ${OBJECTS}
