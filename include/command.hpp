#pragma once

#include <iostream>
#include <string>
#include <vector>

using namespace std;

class Command {
public:
    string name;
    vector<string> argv;

    string inputFile;
    string outputFile;

    string errorFile;

    bool appendOutput;
};