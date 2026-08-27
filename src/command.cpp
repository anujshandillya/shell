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

Command::Command(const Command& other) {
    name = other.name ? strdup(other.name) : nullptr;
    if (other.argv) {
        int count = 0;
        while (other.argv[count] != nullptr) {
            count++;
        }
        argv = new char*[count + 1];
        for (int i = 0; i < count; i++) {
            argv[i] = strdup(other.argv[i]);
        }
        argv[count] = nullptr;
    } else {
        argv = nullptr;
    }
    inputFile = other.inputFile ? strdup(other.inputFile) : nullptr;
    outputFile = other.outputFile ? strdup(other.outputFile) : nullptr;
    errorFile = other.errorFile ? strdup(other.errorFile) : nullptr;
    appendOutput = other.appendOutput;
}

Command& Command::operator=(const Command& other) {
    if (this == &other) {
        return *this;
    }

    if (name) free(name);
    if (argv) {
        for (int i = 0; argv[i] != nullptr; i++) {
            free(argv[i]);
        }
        delete[] argv;
    }
    if (inputFile) free(inputFile);
    if (outputFile) free(outputFile);
    if (errorFile) free(errorFile);

    name = other.name ? strdup(other.name) : nullptr;
    if (other.argv) {
        int count = 0;
        while (other.argv[count] != nullptr) {
            count++;
        }
        argv = new char*[count + 1];
        for (int i = 0; i < count; i++) {
            argv[i] = strdup(other.argv[i]);
        }
        argv[count] = nullptr;
    } else {
        argv = nullptr;
    }
    inputFile = other.inputFile ? strdup(other.inputFile) : nullptr;
    outputFile = other.outputFile ? strdup(other.outputFile) : nullptr;
    errorFile = other.errorFile ? strdup(other.errorFile) : nullptr;
    appendOutput = other.appendOutput;

    return *this;
}

Command::~Command() {
    if (name) {
        free(name);
        name = nullptr;
    }
    if (argv) {
        for (int i = 0; argv[i] != nullptr; i++) {
            free(argv[i]);
        }
        delete[] argv;
        argv = nullptr;
    }
    if (inputFile) {
        free(inputFile);
        inputFile = nullptr;
    }
    if (outputFile) {
        free(outputFile);
        outputFile = nullptr;
    }
    if (errorFile) {
        free(errorFile);
        errorFile = nullptr;
    }
}
