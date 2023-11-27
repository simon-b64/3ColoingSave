#/**
# * @file Makefile
# * @author Simon Buchinger 12220026 <e12220026@student.tuwien.ac.at>
# * @date 27.11.2023
# * @program: 3coloring
# *
# **/

# $@ => Target
# $^ => All dependecies
# $< => first dependency

CC := gcc
CFLAGS := -std=c99 -pedantic -Wall -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_POSIX_C_SOURCE=200809L -g

.PHONY: all
all: supervisor  generator

.PHONY: clean
clean:
	rm -rf ./*.o supervisor generator

supervisor: supervisor.o
	$(CC) $(CFLAGS) -o $@ $^

generator: generator.o
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<