#include "parser.hpp"

#include <cstring>
#include <cstdlib>

Command **Parser::parse(char *input) {
    if (input == nullptr || input[0] == '\0') {
        return nullptr;
    }

    // command count
    int commandCount = 1;
    for (int i = 0; input[i] != '\0'; i++) {
        if (input[i] == '|' || input[i] == ';') {
            commandCount++;
        }
    }
    char **commands = new char*[commandCount + 1];
    Command **commandObjects = new Command*[commandCount + 1];
    int idx = 0;

    // separate the commands by the pipe symbol '|' and ';'
    char *command = strtok(input, "|;");
    commands[idx++] = command;
    while ((command = strtok(nullptr, "|;")) != nullptr) {
        commands[idx++] = command;
    }

    commands[idx] = nullptr;

    int objCount = 0;   // tracks how many Command* we've actually stored

    for (int i = 0; commands[i] != nullptr; i++) {
        char *cmd = commands[i];
        char *cmdCopy = strdup(cmd);
        char *token = strtok(cmdCopy, " \t");
        if (token == nullptr) {
            free(cmdCopy);
            continue;
        }
        char *commandName = strdup(token);
        int argCount = 0;
        while ((token = strtok(nullptr, " \t")) != nullptr) {
            argCount++;
        }
        free(cmdCopy);   // done with cmdCopy, was only needed for counting

        char **arguments = new char*[argCount + 2];   // name + args + null
        arguments[0] = commandName;
        int argIdx = 1;   // renamed from reusing `idx`
        token = strtok(cmd, " \t");
        while ((token = strtok(nullptr, " \t")) != nullptr) {
            arguments[argIdx++] = strdup(token);
        }
        arguments[argIdx] = nullptr;

        Command *commandObj = new Command(commandName, arguments);
        commandObjects[objCount++] = commandObj;
    }

    commandObjects[objCount] = nullptr;   // was commandObjects[idx]
    return commandObjects;
}