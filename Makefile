CFLAGS=-g -Wall `pkg-config --cflags gtk+-2.0` `pkg-config --cflags libpulse`
LDLIBS=`pkg-config --libs gtk+-2.0` `pkg-config --libs libpulse`

all: te

