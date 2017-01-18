CC = g++

CXXFLAGS = $(shell pkg-config --cflags opencv)
LDLIBS = $(shell pkg-config --libs opencv)

objects = main.cpp serial.cpp serial.h flower.cpp flower.h camera.cpp camera.h

bow : $(objects)
	$(CC) -o bow $(objects) $(CXXFLAGS) $(LDLIBS) -lpthread

clean:
	/bin/rm -f bow *.o

clean-all: clean
	/bin/rm -f *~ 
