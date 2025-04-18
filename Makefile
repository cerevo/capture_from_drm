CC=g++

OBJS = main.o capture_drm.o
TARGET = capture_drm_sample

LIBS = -lm -lpthread
LIBS += `pkg-config --cflags --libs libdrm`
CFLAGS = -I/usr/include
CPPFLAGS = -std=c++11 -Wall -g -O0
CPPFLAGS += `pkg-config --cflags --libs libdrm`

.PHONY: all clean install

all: $(TARGET) $(OBJS)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

clean:
	rm $(TARGET) $(OBJS)
