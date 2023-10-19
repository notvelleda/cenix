CC=cc
CFLAGS=-I. -I../include -std=c89 -Os
LDFLAGS=

BINARY=parser
OBJECTS=lexer.o main.o parser.o hashtable.o ir.o type.o output.o

all: ${BINARY}

%.o: %.c
	${CC} ${CFLAGS} -c $<

${BINARY}: ${OBJECTS}
	${CC} ${LDFLAGS} ${OBJECTS} -o ${BINARY}

clean:
	-rm ${BINARY} ${OBJECTS}
