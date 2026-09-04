#pragma once

#include <cstddef>
#include <string>
#include <vector>

class History {
public:
    History();
    void add(const char *command);

    const char *previous();
    const char *next();

    void print();
    ~History();

private:
    static constexpr std::size_t maxCommands = 20;
    std::vector<std::string> commands;
    std::size_t currentIndex;
    int fd;
};