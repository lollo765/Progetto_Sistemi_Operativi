CC = gcc
CFLAGS = -w -g -Wall -Wextra -Werror -pedantic -std=c11
LDFLAGS = -lncurses -pthread
SRC = main.c pid_info.c pid_stats.c linked_list.c
OBJ = $(SRC:.c=.o)
TARGET = top

.PHONY: all clean

all: $(TARGET)
	$(MAKE) clean && echo "Compilation successful."

$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -rf *.o *~ $(OBJ)
