#include "command.hpp"
#include <cstdlib>
#include <cstring>

Command::Command() {
    name = nullptr;
    argv = nullptr;
    inputFile = nullptr;
    outputFile = nullptr;
    errorFile = nullptr;
    
    appendOutput = false;
    pipeToNext = false;
}

Command::Command(char *commandName, char **arguments) {
    name = strdup(commandName);
    argv = arguments;
    inputFile = nullptr;
    outputFile = nullptr;
    errorFile = nullptr;
    
    appendOutput = false;
    pipeToNext = false;
}