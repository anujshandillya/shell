#include "parser.hpp"

#include <cstring>
#include <cstdlib>

static int splitOnSeparators(char* input, char** outCommands, char* outSeparators) {
    int count = 0;
    char* p = input;
    char* segStart = p;

    while (*p != '\0') {
        if (*p == '"' || *p == '\'') {
            char quoteChar = *p;
            p++;
            while (*p != '\0' && *p != quoteChar) p++;
            if (*p == quoteChar) p++;
            continue;
        }

        if (*p == '|' || *p == ';') {
            char sep = *p;
            *p = '\0';
            outCommands[count] = segStart;
            outSeparators[count] = sep;
            count++;
            p++;
            segStart = p;
            continue;
        }

        p++;
    }

    if (p != segStart) {
        outCommands[count] = segStart;
        outSeparators[count] = '\0';
        count++;
    }

    return count;
}

static int tokenizeArgs(char* input, char** outTokens) {
    int count = 0;
    char* p = input;

    while (*p != '\0') {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') break;

        char* tokenStart = p;
        char* writePtr = p;

        if (*p == '"' || *p == '\'') {
            char quoteChar = *p;

            *writePtr++ = *p++;              // keep opening quote in the token

            while (*p != '\0' && *p != quoteChar) {
                *writePtr++ = *p++;           // copy content, spaces included
            }

            if (*p == quoteChar) {
                *writePtr++ = *p++;           // keep closing quote in the token
            }

            *writePtr = '\0';
        } else {
            while (*p != '\0' && *p != ' ' && *p != '\t') p++;
            if (*p != '\0') {
                *p = '\0';
                p++;
            }
        }

        outTokens[count++] = tokenStart;
    }

    return count;
}

static int extractRedirection(char** tokens, int tokenCount, Command* cmd) {
    int writeIdx = 0;
    for (int i = 0; i < tokenCount; i++) {
        if (strcmp(tokens[i], ">") == 0 && i + 1 < tokenCount) {
            cmd->outputFile = strdup(tokens[i + 1]);
            cmd->appendOutput = false;
            i++;
        } else if (strcmp(tokens[i], ">>") == 0 && i + 1 < tokenCount) {
            cmd->outputFile = strdup(tokens[i + 1]);
            cmd->appendOutput = true;
            i++;
        } else if (strcmp(tokens[i], "<") == 0 && i + 1 < tokenCount) {
            cmd->inputFile = strdup(tokens[i + 1]);
            i++;
        } else if (strcmp(tokens[i], "2>") == 0 && i + 1 < tokenCount) {
            cmd->errorFile = strdup(tokens[i + 1]);
            i++;
        } else {
            tokens[writeIdx++] = tokens[i];
        }
    }
    return writeIdx;
}

Command **Parser::parse(char *input) {
    if (input == nullptr || input[0] == '\0') {
        return nullptr;
    }

    int maxCommands = 1;
    for (int i = 0; input[i] != '\0'; i++) {
        if (input[i] == '|' || input[i] == ';') {
            maxCommands++;
        }
    }

    char **commands = new char*[maxCommands + 1];
    char *separators = new char[maxCommands + 1];

    int commandCount = splitOnSeparators(input, commands, separators);

    if (commandCount == 0) {
        delete[] commands;
        delete[] separators;
        return nullptr;
    }

    Command **commandObjects = new Command*[commandCount + 1];
    int objCount = 0;

    for (int i = 0; i < commandCount; i++) {
        int maxTokens = (int)strlen(commands[i]) / 2 + 2;
        char **tokens = new char*[maxTokens];

        int tokenCount = tokenizeArgs(commands[i], tokens);

        if (tokenCount == 0) {
            delete[] tokens;
            continue;
        }

        char *commandName = strdup(tokens[0]);

        char **arguments = new char*[tokenCount + 1];
        arguments[0] = commandName;
        for (int j = 1; j < tokenCount; j++) {
            arguments[j] = strdup(tokens[j]);
        }
        arguments[tokenCount] = nullptr;

        Command *commandObj = new Command(commandName, arguments, tokenCount);

        int newArgc = extractRedirection(arguments, tokenCount, commandObj);
        arguments[newArgc] = nullptr;
        commandObj->argc = newArgc;

        commandObj->pipeToNext = (separators[i] == '|');

        commandObjects[objCount++] = commandObj;

        delete[] tokens;
    }

    commandObjects[objCount] = nullptr;

    delete[] commands;
    delete[] separators;

    return commandObjects;
}