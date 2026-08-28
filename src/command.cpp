#include "command.hpp"
#include <cstdlib>
#include <cstring>

Command::Command() {
    name = nullptr;
    argv = nullptr;
    argc = 0;
    inputFile = nullptr;
    outputFile = nullptr;
    errorFile = nullptr;
    
    appendOutput = false;
    pipeToNext = false;
}

Command::Command(char *commandName, char **arguments, int argumentCount) {
    name = strdup(commandName);
    argv = arguments;
    argc = argumentCount;
    inputFile = nullptr;
    outputFile = nullptr;
    errorFile = nullptr;
    
    appendOutput = false;
    pipeToNext = false;
}