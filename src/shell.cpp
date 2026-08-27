#include "shell.hpp"
#include "parser.hpp"
#include "history.hpp"

#include <iostream>
#include <string>
#include <csignal>
#include <unistd.h>
#include <cstdlib>

using namespace std;

void freeCommands(Command **commands) {
    if (commands == nullptr) {
        return;
    }

    for (int i = 0; commands[i] != nullptr; i++) {
        delete commands[i];
    }

    delete[] commands;
}

Shell::Shell() : is_running(true) {
    cout << "Running...\n";
    signal(SIGINT, signal_handler);
    const char* user = getenv("USER");
    username = user;
    
    if(gethostname(Shell::host, sizeof(Shell::host)) != 0) {
        strncpy(Shell::host, "localhost", sizeof(Shell::host));
    }
    Shell::home_directory = "~";
    Shell::parser = new Parser();
}

Shell::~Shell() {
    delete parser;
}

void Shell::Print() {
    cout << "<" << Shell::username << "@" << Shell::host << ":~> ";
    cout.flush();    
}

void Shell::signal_handler(int signum) {
    cout << "Closing Shell...";
    cout.flush();
    // Shell::is_running = false;
}

void Shell::process_input(char *input) {
    char *inputCopy = strdup(input);
    Command **parsedCommands = parser->parse(input);

    if (parsedCommands != nullptr) {
        for (int i = 0; parsedCommands[i] != nullptr; i++) {
            Command *command = parsedCommands[i];

            if(command->pipeToNext) {
                // connect stdout of this command to stdin of the next command.
                cout << command->name << " will pipe to next command." << endl;
            }else {
                // Execute the command
                cout << command->name << " will execute." << endl;
            }
        }

        freeCommands(parsedCommands);
    }

    Shell::history.add(inputCopy);
    if(strcmp(input, "exit") == 0) {
        Shell::is_running = false;
        return;
    }
}

void Shell::run() {
    while (is_running) {
        Print();
        char input[1024];

        if (fgets(input, sizeof(input), stdin) != NULL) {
            input[strcspn(input, "\n")] = '\0';   
        }

        cout << "You entered: " << input << endl;
        Shell::process_input(input);
    }
}