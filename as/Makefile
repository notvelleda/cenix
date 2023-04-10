CC=cc
CFLAGS=-I. -I../include -std=c89
LDFLAGS=

BINARY=as
OBJECTS=y.tab.o lex.yy.o main.o

all: ${BINARY}

y.tab.c: as.y as.h
	yacc -d as.y

lex.yy.c: as.l y.tab.c
	lex as.l

%.o: %.c
	${CC} ${CFLAGS} -c $<

${BINARY}: ${OBJECTS}
	${CC} ${LDFLAGS} ${OBJECTS} -o ${BINARY}

clean:
	-rm ${BINARY} ${OBJECTS} y.tab.c y.tab.h lex.yy.c
