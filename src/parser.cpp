#include "parser.hpp"

#include <cstring>
#include <cstdlib>

Command **Parser::parse(char *input) {
    if (input == nullptr || input[0] == '\0') {
        return nullptr;
    }

    int commandCount = 1;

    for (int i = 0; input[i] != '\0'; i++) {
        if (input[i] == '|' || input[i] == ';') {
            commandCount++;
        }
    }

    Command **commands = new Command*[commandCount + 1];

    int commandIndex = 0;
    char *start = input;

    for (int i = 0;; i++) {
        char separator = input[i];

        if (separator != '|' && separator != ';' && separator != '\0') {
            continue;
        }

        input[i] = '\0';

        char *save = nullptr;
        char *argument = strtok_r(start, " \t\n", &save);

        if (argument != nullptr) {
            Command *command = new Command();

            int argumentCount = 1;
            char *scan = argument;

            while ((scan = strtok_r(nullptr, " \t\n", &save)) != nullptr) {
                argumentCount++;
            }

            command->argv = new char*[argumentCount + 1];

            save = nullptr;
            argument = strtok_r(start, " \t\n", &save);

            int argumentIndex = 0;

            while (argument != nullptr) {
                command->argv[argumentIndex] = strdup(argument);

                if (argumentIndex == 0) {
                    command->name = strdup(argument);
                }

                argumentIndex++;
                argument = strtok_r(nullptr, " \t\n", &save);
            }

            command->argv[argumentCount] = nullptr;
            command->pipeToNext = separator == '|';

            commands[commandIndex] = command;
            commandIndex++;
        }

        if (separator == '\0') {
            break;
        }

        start = input + i + 1;
    }

    commands[commandIndex] = nullptr;
    return commands;
}