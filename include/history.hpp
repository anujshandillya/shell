#pragma once

#include <iostream>
#include <vector>
#include <string>

using namespace std;

class History {
public:
    History();
    void add(const string& command);

    const string& previous();
    const string& next();

    void print();
    ~History();
private:
    vector<string> commands;
    int currentIndex;
    int fd;
};