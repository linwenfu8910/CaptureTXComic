CC = gcc
LIB = -L/usr/local/json/lib -L/usr/local/curl/lib
INCLUDE = -I/usr/local/json/include/json -I/usr/local/curl/include
CURLFLAGS = -lcurl 
JSONFLAGS = -ljson
PTHREADFLAGS = -lpthread
CFLAGS = -O2 -std=gnu99 $(INCLUDE) $(LIB)
DIR = .
FILES = $(foreach dir, $(DIR), $(wildcard $(dir)/*.c))
OBJS = $(patsubst %.c, %.o, $(FILES))
RM = rm -f
TARGET = CaptureTXComic
$(TARGET) : $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(CURLFLAGS) $(JSONFLAGS) $(PTHREADFLAGS) -o $(TARGET)
clean:
	-$(RM) $(TARGET)
	-$(RM) $(OBJS)
