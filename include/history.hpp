#pragma once

#include <cstddef>
#include <sys/types.h>

class History {
public:
    History();
    void add(const char *command);

    const char *previous();
    const char *next();

    void print();
    ~History();

private:
    static const int maxCommands = 20;
    char *commands[maxCommands];
    int commandCount;
    int currentIndex;
    int fd;

    ssize_t readLine(char *line, size_t capacity);
};