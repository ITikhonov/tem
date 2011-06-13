CFLAGS=-g -Wall `pkg-config --cflags gtk+-2.0`
LDLIBS=`pkg-config --libs gtk+-2.0`

all: te

