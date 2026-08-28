#include "command_handler.hpp"
#include "shell.hpp"

#include <iostream>
#include <unistd.h>

using namespace std;

void CommandHandler::execute(const Command& command) {
    if (strcmp(command.name, "cd") == 0) {
        cd(command);
    } else if (strcmp(command.name, "ls") == 0) {
        ls(command);
    } else if (strcmp(command.name, "pwd") == 0) {
        pwd(command);
    } else if (strcmp(command.name, "cat") == 0) {
        cat(command);
    } else if (strcmp(command.name, "search") == 0) {
        search(command);
    } else if (strcmp(command.name, "grep") == 0) {
        grep(command);
    } else if (strcmp(command.name, "history") == 0) {
        history(command);
    } else if (strcmp(command.name, "pinfo") == 0) {
        pinfo(command);
    } else if (strcmp(command.name, "echo") == 0) {
        echo(command);
    } else {
        cout << "Unknown command: " << command.name << endl;
        return;
    }
}

// cd
void CommandHandler::cd(const Command& command) {
    if(command.argc > 2) {
        perror("Invalid Arguments");
        return;
    }
    cout << "Executing cd command" << endl;
    char* path = command.argv[1];

    if (path == nullptr) {
        path = getenv("HOME");
    }

    if (path == nullptr || strcmp(path, "~") == 0 || strcmp(path, "") == 0) {
        path = getenv("HOME");
        if (path == nullptr) {
            perror("HOME not set");
            return;
        }
    } else if (strcmp(path, "-") == 0) {
        path = getenv("OLDPWD");
        if (path == nullptr) {
            perror("OLDPWD not set");
            return;
        }
        printf("OLDPWD: %s\n", path);
    }
    
    char target[PATH_MAX];
    strncpy(target, path, sizeof(target) - 1);
    target[sizeof(target) - 1] = '\0';

    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == nullptr) {
        perror("getcwd() error");
        return;
    }

    printf("Changing directory to: %s\n", target);

    if (chdir(target) != 0) {
        perror("cd failed");
        return;
    }

    setenv("OLDPWD", cwd, 1);
    printf("OLDPWD set to: %s\n", cwd);
}

// ls
void CommandHandler::ls(const Command& command) {
    printf("Executing %s command\n", command.name);
    return;
}

// pwd
void CommandHandler::pwd(const Command& command) {
    printf("Executing %s command\n", command.name);
    return;
}

// cat
void CommandHandler::cat(const Command& command) {
    printf("Executing %s command\n", command.name);
    return;
}

// search
void CommandHandler::search(const Command& command) {
    printf("Executing %s command\n", command.name);
    return;
}

// grep
void CommandHandler::grep(const Command& command) {
    printf("Executing %s command\n", command.name);
    return;
}

// history
void CommandHandler::history(const Command& command) {
    printf("Executing %s command\n", command.name);
    return;
}

// pinfo
void CommandHandler::pinfo(const Command& command) {
    printf("Executing %s command\n", command.name);
    return;
}

// echo
void CommandHandler::echo(const Command& command) {
    printf("Executing %s command\n", command.name);
    return;
}