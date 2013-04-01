TARGET = sistema-ficheros

CC = gcc
CFLAGS = -g -Wall 
LDFLAGS = -lreadline

OBJS = common.o parse.o util.o MiSistemaDeFicheros.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGET)  $(OBJS)

.c.o: 
	$(CC) $(CFLAGS) -I. -c  $<

clean: 
	-rm -f *.o $(TARGET) 
