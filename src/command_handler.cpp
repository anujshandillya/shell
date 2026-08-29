#include "command_handler.hpp"
#include "shell.hpp"

#include <iostream>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <grp.h>
#include <pwd.h>

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
    printf("Executing %s command\n", command.name);
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

static void printPermissions(mode_t mode) {
    char perms[11];
    perms[0] = S_ISDIR(mode) ? 'd' : (S_ISLNK(mode) ? 'l' : '-');
    perms[1] = (mode & S_IRUSR) ? 'r' : '-';
    perms[2] = (mode & S_IWUSR) ? 'w' : '-';
    perms[3] = (mode & S_IXUSR) ? 'x' : '-';
    perms[4] = (mode & S_IRGRP) ? 'r' : '-';
    perms[5] = (mode & S_IWGRP) ? 'w' : '-';
    perms[6] = (mode & S_IXGRP) ? 'x' : '-';
    perms[7] = (mode & S_IROTH) ? 'r' : '-';
    perms[8] = (mode & S_IWOTH) ? 'w' : '-';
    perms[9] = (mode & S_IXOTH) ? 'x' : '-';
    perms[10] = '\0';
    printf("%s ", perms);
}

static void listOneDirectory(const char* dirPath, bool a_flag, bool l_flag, bool printHeader) {
    DIR* dir = opendir(dirPath);
    if (dir == nullptr) {
        perror(dirPath);
        return;
    }

    if (printHeader) {
        printf("%s:\n", dirPath);
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        // Skip hidden files unless -a is set
        if (!a_flag && entry->d_name[0] == '.') {
            continue;
        }

        if (l_flag) {
            char fullPath[PATH_MAX];
            snprintf(fullPath, sizeof(fullPath), "%s/%s", dirPath, entry->d_name);

            struct stat st;
            if (lstat(fullPath, &st) != 0) {
                perror(fullPath);
                continue;
            }

            printPermissions(st.st_mode);
            printf("%2ld ", (long)st.st_nlink);

            struct passwd* pw = getpwuid(st.st_uid);
            struct group* gr = getgrgid(st.st_gid);
            printf("%s ", pw != nullptr ? pw->pw_name : "?");
            printf("%s ", gr != nullptr ? gr->gr_name : "?");

            printf("%8lld ", (long long)st.st_size);

            char timeBuf[64];
            struct tm* tm_info = localtime(&st.st_mtime);
            strftime(timeBuf, sizeof(timeBuf), "%b %d %H:%M", tm_info);
            printf("%s ", timeBuf);

            printf("%s\n", entry->d_name);
        } else {
            printf("%s  ", entry->d_name);
        }
    }

    if (!l_flag) {
        printf("\n");
    }

    closedir(dir);
}

void CommandHandler::ls(const Command& command) {
    printf("Executing %s command\n", command.name);

    bool a_flag = false, l_flag = false;

    char* directories[256]; // adjust size or make dynamic as needed
    int dirCount = 0;

    for (int i = 1; command.argv[i] != nullptr; i++) {
        char* arg = command.argv[i];

        if (strcmp(arg, "-l") == 0) {
            l_flag = true;
        } else if (strcmp(arg, "-a") == 0) {
            a_flag = true;
        } else if (strcmp(arg, "-al") == 0 || strcmp(arg, "-la") == 0) {
            l_flag = true;
            a_flag = true;
        } else if (arg[0] == '-') {
            printf("ls: invalid option -- '%s'\n", arg);
        } else {
            if (dirCount < 256) {
                directories[dirCount++] = arg;
            }
        }
    }

    if (dirCount == 0) {
        listOneDirectory(".", a_flag, l_flag, false);
    } else {
        for (int i = 0; i < dirCount; i++) {
            listOneDirectory(directories[i], a_flag, l_flag, dirCount > 1);
            if (dirCount > 1 && i < dirCount - 1) {
                printf("\n");
            }
        }
    }
}

// pwd
void CommandHandler::pwd(const Command& command) {
    printf("Executing %s command\n", command.name);

    if (command.argc > 1) {
        perror("Invalid Arguments");
        return;
    }

    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == nullptr) {
        perror("getcwd() error");
        return;
    }

    printf("%s\n", cwd);
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