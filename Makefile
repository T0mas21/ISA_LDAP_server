CC = gcc
CFLAGS = -Wall -Wextra -std=c11
LDFLAGS = -lm
TARGET = isa-ldapserver
SRC = isa-ldapserver.c

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET)