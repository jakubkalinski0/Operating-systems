CC = gcc
CFLAGS = -Wall -Wextra -g -std=c11
LDFLAGS =

TARGET_SERVER = server
TARGET_CLIENT = client

all: $(TARGET_SERVER) $(TARGET_CLIENT)

$(TARGET_SERVER): server.c common.h
	$(CC) $(CFLAGS) server.c -o $(TARGET_SERVER) $(LDFLAGS)

$(TARGET_CLIENT): client.c common.h
	$(CC) $(CFLAGS) client.c -o $(TARGET_CLIENT) $(LDFLAGS)

clean:
	rm -f $(TARGET_SERVER) $(TARGET_CLIENT)