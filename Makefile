CC = gcc
LIB = -L/usr/local/json/lib
INCLUDE = -I/usr/local/json/include/json
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
	$(CC) -o $(TARGET) $(OBJS) $(CURLFLAGS) $(JSONFLAGS) $(PTHREADFLAGS)
clean:
	-$(RM) $(TARGET)
	-$(RM) $(OBJS)
