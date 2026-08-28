#include "shell.hpp"
#include "parser.hpp"
#include "history.hpp"

#include <iostream>
#include <string>
#include <csignal>
#include <unistd.h>
#include <cstdlib>

using namespace std;

volatile sig_atomic_t Shell::is_running = true;
volatile sig_atomic_t Shell::is_processing_command = false;

void freeCommands(char **commands) {
    if (commands == nullptr) {
        return;
    }

    for (int i = 0; commands[i] != nullptr; i++) {
        delete commands[i];
    }

    delete[] commands;
}

Shell::Shell() {
    cout << "Running...\n";
    signal(SIGINT, signal_handler);
    const char* user = getenv("USER");
    if (user != nullptr) {
        username = user;
    } else {
        username = "user";
    }
    
    if(gethostname(Shell::host, sizeof(Shell::host)) != 0) {
        strncpy(Shell::host, "localhost", sizeof(Shell::host));
    }

    char root[PATH_MAX];
    if (getcwd(root, sizeof(root)) != nullptr) {
        Shell::current_directory = root;
    } else {
        Shell::current_directory = "";
    }

    Shell::parser = new Parser();
    Shell::handler = new CommandHandler();
}

Shell::~Shell() {
    delete parser;
    delete handler;
}

void Shell::Print() {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == nullptr) {
        strcpy(cwd, "?");
    } else {
        const char *root = Shell::current_directory.c_str();
        size_t root_len = strlen(root);

        if (root_len > 0) {
            if (strcmp(cwd, root) == 0) {
                strcpy(cwd, "~");
            } else if (strncmp(cwd, root, root_len) == 0 && (root[root_len - 1] == '/' || cwd[root_len] == '/')) {
                char temp[PATH_MAX];
                if (strcmp(root, "/") == 0) {
                    snprintf(temp, sizeof(temp), "~%s", cwd);
                } else {
                    snprintf(temp, sizeof(temp), "~%s", cwd + root_len);
                }
                strncpy(cwd, temp, sizeof(cwd) - 1);
                cwd[sizeof(cwd) - 1] = '\0';
            }
        }
    }
    printf("<%s@%s:%s> ", Shell::username.c_str(), Shell::host, cwd);
    cout.flush();
}

void Shell::signal_handler(int signum) {
    cout.flush();
    if(Shell::is_processing_command) {
        cout << "\nCommand processing interrupted. Press Ctrl+C again to exit.\n";
        Shell::is_processing_command = false;
        return;
    }
    Shell::is_running = false;
    exit(signum);
}

void Shell::process_input(char *input) {
    char *inputCopy = strdup(input);
    Command **parsedCommands = parser->parse(input);

    if(parsedCommands == nullptr) {
        free(inputCopy);
        return;
    }

    for (int i = 0; parsedCommands[i] != nullptr; i++) {
        Command *cmd = parsedCommands[i];
        
        printf("Command %d: %s\n", i + 1, cmd->name);

        handler->execute(*cmd);
        // free(command);
    }

    // freeCommands(parsedCommands);

    Shell::history.add(inputCopy);

    bool shouldExit = (strcmp(input, "exit") == 0);

    delete[] parsedCommands; // free array of pointers
    free(inputCopy);

    if (shouldExit) {
        Shell::is_running = false;
    }
}

void Shell::run() {
    while (is_running) {
        Print();
        char input[1024];

        if (fgets(input, sizeof(input), stdin) != NULL) {
            input[strcspn(input, "\n")] = '\0';   
        }else {
            cout.flush();
            break;
        }
        
        cout << "You entered: " << input << endl;
        is_processing_command = true;
        Shell::process_input(input);
        // sleep(10); // Simulate a delay for testing signal handling
        is_processing_command = false;
    }
}