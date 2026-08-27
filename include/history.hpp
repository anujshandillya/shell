#pragma once

#include <iostream>

using namespace std;

class History {
public:
    History();
    void add(const char *command);

    const char *previous();
    const char *next();

    void print();
    ~History();
private:
    char **commands;
    int currentIndex;
    int fd;
};