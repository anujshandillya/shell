#pragma once

#include <iostream>

using namespace std;

class Command {
public:
    char *name;
    char **argv;

    char *inputFile;
    char *outputFile;
    char *errorFile;

    bool appendOutput;
    bool pipeToNext;

    Command();
    Command(char *commandName, char **arguments);
    ~Command();
};