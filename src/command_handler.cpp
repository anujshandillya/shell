#include "command_handler.hpp"
#include "shell.hpp"

#include <iostream>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <grp.h>
#include <pwd.h>
#include <fcntl.h>
#include <regex.h>
#include <sys/wait.h>
#include <signal.h>

using namespace std;
struct SavedFds {
    int savedStdout = -1;
    int savedStdin  = -1;
    int savedStderr = -1;
};


int CommandHandler::lastExitStatus = 0;

static bool applyRedirection(const Command &cmd, SavedFds& saved) {
    if (cmd.outputFile != nullptr) {
        int flags = O_WRONLY | O_CREAT | (cmd.appendOutput ? O_APPEND : O_TRUNC);
        int fd = open(cmd.outputFile, flags, 0644);
        if (fd == -1) {
            perror(cmd.outputFile);
            return false;
        }
        saved.savedStdout = dup(STDOUT_FILENO);
        // Redirect stdout to the output file
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }

    if (cmd.inputFile != nullptr) {
        int fd = open(cmd.inputFile, O_RDONLY);
        if (fd == -1) {
            perror(cmd.inputFile);
            return false;
        }
        saved.savedStdin = dup(STDIN_FILENO);
        // Redirect stdin to the input file
        dup2(fd, STDIN_FILENO);
        close(fd);
    }

    if (cmd.errorFile != nullptr) {
        int flags = O_WRONLY | O_CREAT | (cmd.appendOutput ? O_APPEND : O_TRUNC);
        int fd = open(cmd.errorFile, flags, 0644);
        if (fd == -1) {
            perror(cmd.errorFile);
            return false;
        }
        saved.savedStderr = dup(STDERR_FILENO);
        // Redirect stderr to the error file
        dup2(fd, STDERR_FILENO);
        close(fd);
    }

    return true;
}

static void restoreRedirection(SavedFds& saved) {
    if (saved.savedStdout != -1) {
        dup2(saved.savedStdout, STDOUT_FILENO);
        close(saved.savedStdout);
    }
    if (saved.savedStdin != -1) {
        dup2(saved.savedStdin, STDIN_FILENO);
        close(saved.savedStdin);
    }
    if (saved.savedStderr != -1) {
        dup2(saved.savedStderr, STDERR_FILENO);
        close(saved.savedStderr);
    }
}

bool CommandHandler::executeBuiltin(const Command& command) {
    if (strcmp(command.name, "cd") == 0) {
        cd(command);
    } else if (strcmp(command.name, "ls") == 0) {
        ls(command);
    } else if (strcmp(command.name, "pwd") == 0) {
        pwd(command);
    } else if (strcmp(command.name, "search") == 0) {
        search(command);
    } else if (strcmp(command.name, "history") == 0) {
        history(command);
    } else if (strcmp(command.name, "pinfo") == 0) {
        pinfo(command);
    } else if (strcmp(command.name, "echo") == 0) {
        echo(command);
    } else {
        return false;
    }
    return true;
}

void CommandHandler::execute(const Command& command) {
    if (strcmp(command.name, "cd") != 0 &&
        strcmp(command.name, "ls") != 0 &&
        strcmp(command.name, "pwd") != 0 &&
        strcmp(command.name, "search") != 0 &&
        strcmp(command.name, "history") != 0 &&
        strcmp(command.name, "pinfo") != 0 &&
        strcmp(command.name, "echo") != 0) {
        executeExternal(const_cast<Command*>(&command));
        return;
    }

    SavedFds saved;
    if (!applyRedirection(command, saved)) {
        return;
    }

    executeBuiltin(command);
    restoreRedirection(saved);
}

// cd
void CommandHandler::cd(const Command& command) {
    if(command.argc > 2) {
        perror("Invalid Arguments");
        return;
    }

    char* path = command.argv[1];
    // if no path provided, default to HOME directory.
    if (path == nullptr) {
        path = getenv("HOME");
    }

    if (path == nullptr || strcmp(path, "~") == 0 || strcmp(path, "") == 0) {
        // Handle special cases for path: "~" and ""
        path = getenv("HOME");
        if (path == nullptr) {
            perror("HOME not set");
            return;
        }
    } else if (strcmp(path, "-") == 0) {
        // Handle special case for path: "-" previous directory
        path = getenv("OLDPWD");
        if (path == nullptr) {
            perror("OLDPWD not set");
            return;
        }
    }
    
    // Copy the path to a local buffer to avoid modifying the original string
    char target[PATH_MAX];
    strncpy(target, path, sizeof(target) - 1);
    target[sizeof(target) - 1] = '\0';

    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == nullptr) {
        perror("getcwd() error");
        return;
    }

    if (chdir(target) != 0) {
        perror("cd failed");
        return;
    }

    // setting the environemnt variable to the previous current working directory after chdir()
    setenv("OLDPWD", cwd, 1);
}

// ls
static void printPermissions(mode_t mode) {
    char perms[11];
    // is it a directory, regular file, or symbolic link?
    perms[0] = S_ISDIR(mode) ? 'd' : (S_ISLNK(mode) ? 'l' : '-');

    // permissions for owner
    perms[1] = (mode & S_IRUSR) ? 'r' : '-';
    perms[2] = (mode & S_IWUSR) ? 'w' : '-';
    perms[3] = (mode & S_IXUSR) ? 'x' : '-';

    // permissions for group
    perms[4] = (mode & S_IRGRP) ? 'r' : '-';
    perms[5] = (mode & S_IWGRP) ? 'w' : '-';
    perms[6] = (mode & S_IXGRP) ? 'x' : '-';

    // permissions for others
    perms[7] = (mode & S_IROTH) ? 'r' : '-';
    perms[8] = (mode & S_IWOTH) ? 'w' : '-';
    perms[9] = (mode & S_IXOTH) ? 'x' : '-';
    perms[10] = '\0';
    printf("%s ", perms);
}

// List the contents of a single directory
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

            // get user and group names from uid and gid
            struct passwd* pw = getpwuid(st.st_uid);
            struct group* gr = getgrgid(st.st_gid);
            printf("%s ", pw != nullptr ? pw->pw_name : "?");
            printf("%s ", gr != nullptr ? gr->gr_name : "?");

            printf("%8lld ", (long long)st.st_size); // Print file size

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

    bool a_flag = false, l_flag = false;

    char* directories[256];
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
            // return early on invalid option
            return;
        } else {
            if (dirCount < 256) {
                directories[dirCount++] = arg;
            }
        }
    }

    if (dirCount == 0) {
        // If no directories are specified, default to the current directory
        listOneDirectory(".", a_flag, l_flag, false);
    } else {
        // List each specified directory
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

    if (command.argc > 1) {
        perror("Invalid Arguments");
        return;
    }

    char cwd[PATH_MAX];
    // Get the current working directory and print it
    if (getcwd(cwd, sizeof(cwd)) == nullptr) {
        perror("getcwd() error");
        return;
    }

    printf("%s\n", cwd);
}

// search
static bool searchRecursive(const char* dirPath, const char* target) {
    DIR* dir = opendir(dirPath);
    if (dir == nullptr) {
        return false;
    }

    struct dirent* entry;
    bool found = false;

    while (!found && (entry = readdir(dir)) != nullptr) {
        // Skip "." and ".." to avoid infinite recursion
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Check if the current entry matches the target name
        if (strcmp(entry->d_name, target) == 0) {
            found = true;
            break;
        }

        // Build full path for recursion
        char fullPath[PATH_MAX];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", dirPath, entry->d_name);

        struct stat st;
        // If lstat fails, skip this entry (e.g., broken symlink)
        if (lstat(fullPath, &st) != 0) {
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            // Recursively search in subdirectory
            if (searchRecursive(fullPath, target)) {
                found = true;
                break;
            }
        }
    }

    closedir(dir);
    return found;
}

void CommandHandler::search(const Command& command) {

    if (command.argc != 2) {
        perror("search: usage: search <name>\n");
        return;
    }

    const char* target = command.argv[1];

    bool result = searchRecursive(".", target);

    printf("%s\n", result ? "True" : "False");
}

// history
void CommandHandler::history(const Command& command) {

    const char* historyFilePath = getenv("HISFILE");

    bool clear_flag = false;
    long limit = -1; // -1 means "show all"

    for (int i = 1; command.argv[i] != nullptr; i++) {
        char* arg = command.argv[i];

        if (strcmp(arg, "-c") == 0) {
            clear_flag = true;
        } else if (arg[0] >= '0' && arg[0] <= '9') {
            char* endptr;
            long val = strtol(arg, &endptr, 10);
            if (*endptr == '\0' && val > 0) {
                limit = val;
            } else {
                printf("history: invalid argument -- '%s'\n", arg);
            }
        } else {
            printf("history: invalid option -- '%s'\n", arg);
        }
    }

    if (clear_flag) {
        // Clear the history file by truncating it to zero length
        int fd = open(historyFilePath, O_WRONLY | O_TRUNC | O_CREAT, 0644);
        if (fd == -1) {
            perror("history: failed to clear");
            return;
        }
        close(fd);
        printf("History cleared.\n");
        return;
    }

    // read history file
    int fd = open(historyFilePath, O_RDONLY);
    if (fd == -1) {
        perror("history: no history file");
        return;
    }

    char buf[65536]; // NOTE: buffer implementation later
    size_t totalRead = 0;
    ssize_t n;
    while (totalRead < sizeof(buf) - 1 &&
           (n = read(fd, buf + totalRead, sizeof(buf) - 1 - totalRead)) > 0) {
        totalRead += n;
    }
    close(fd);

    if (n == -1) {
        perror("history: read error");
        return;
    }

    buf[totalRead] = '\0';

    // Split into lines, count total first (needed for -n / limit)
    long totalLines = 0;
    for (size_t i = 0; i < totalRead; i++) {
        if (buf[i] == '\n') {
            totalLines++;
        }
    }
    // handle trailing line with no final newline
    if (totalRead > 0 && buf[totalRead - 1] != '\n') {
        totalLines++;
    }

    long startLine = 1;
    if (limit != -1 && limit < totalLines) {
        startLine = totalLines - limit + 1;
    }

    // ---- Walk through lines, print from startLine onward ----
    long currentLine = 1;
    size_t lineStart = 0;

    for (size_t i = 0; i <= totalRead; i++) {
        if (i == totalRead || buf[i] == '\n') {
            if (i > lineStart || i == totalRead) { // has content (skip trailing empty)
                if (currentLine >= startLine && i > lineStart) {
                    char numBuf[16];
                    int numLen = snprintf(numBuf, sizeof(numBuf), "%5ld  ", currentLine);
                    write(STDOUT_FILENO, numBuf, numLen);
                    // write the line content
                    write(STDOUT_FILENO, buf + lineStart, i - lineStart);
                    // end with newline if not already present
                    write(STDOUT_FILENO, "\n", 1);
                }
                currentLine++;
            }
            lineStart = i + 1;
        }
    }
}

// pinfo
// if environment runs macOS, use sysctl and proc_pidinfo to get process info
#ifdef __APPLE__
#include <sys/sysctl.h>
#include <libproc.h>
#endif

void CommandHandler::pinfo(const Command& command) {
    // Check if the number of arguments is valid (0 or 1)
    if (command.argc > 2) {
        perror("Invalid Arguments");
        return;
    }

    pid_t pid;
    if (command.argv[1] == nullptr) {
        pid = getpid();
    } else {
        char* endptr;
        long val = strtol(command.argv[1], &endptr, 10);
        if (*endptr != '\0' || val <= 0) {
            printf("ps: invalid process id: %s\n", command.argv[1]);
            return;
        }
        pid = (pid_t)val;
    }

#ifdef __APPLE__
// if environment runs macOS, use sysctl and proc_pidinfo to get process info
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, pid };
    struct kinfo_proc kp;
    size_t len = sizeof(kp);

    if (sysctl(mib, 4, &kp, &len, nullptr, 0) != 0 || len == 0) {
        printf("ps: no such process: %d\n", pid);
        return;
    }

    char state = '?';
    // Determine the process state based on the kinfo_proc structure
    switch (kp.kp_proc.p_stat) {
        case SRUN:    state = 'R'; break;
        case SSLEEP:  state = 'S'; break;
        case SSTOP:   state = 'T'; break;
        case SZOMB:   state = 'Z'; break;
        case SIDL:    state = 'I'; break;
        default:      state = '?'; break;
    }

    // Get the process group ID and check if it is in the foreground
    pid_t pgrp = kp.kp_eproc.e_pgid;
    bool isForeground = false;
    pid_t termPgrp = tcgetpgrp(STDIN_FILENO);
    if (termPgrp != -1 && termPgrp == pgrp) {
        isForeground = true;
    }

    char stateStr[4];
    snprintf(stateStr, sizeof(stateStr), "%c%s", state, isForeground ? "+" : "");

    // proc_pidinfo can be used to get the virtual memory size of the process
    struct proc_taskinfo taskInfo;
    long vmSizeKb = -1;
    if (proc_pidinfo(pid, PROC_PIDTASKINFO, 0, &taskInfo, sizeof(taskInfo)) > 0) {
        vmSizeKb = (long)(taskInfo.pti_virtual_size / 1024); // bytes -> KB
    }

    // Get the executable path of the process using proc_pidpath
    char exeResolved[PATH_MAX];
    if (proc_pidpath(pid, exeResolved, sizeof(exeResolved)) <= 0) {
        strncpy(exeResolved, "?", sizeof(exeResolved));
    }

    printf("Process Status -- %s\n", stateStr);
    if (vmSizeKb != -1) {
        printf("memory -- %ld {Virtual Memory}\n", vmSizeKb);
    } else {
        printf("memory -- ? {Virtual Memory}\n");
    }
    printf("Executable Path -- %s\n", exeResolved);

#else
    // if environment runs Linux, use /proc filesystem to get process info
    char statPath[64];
    snprintf(statPath, sizeof(statPath), "/proc/%d/stat", pid);

    char statBuf[512];
    if (readFileRaw(statPath, statBuf, sizeof(statBuf)) < 0) {
        printf("ps: no such process: %d\n", pid);
        return;
    }

    char* closeParen = strrchr(statBuf, ')');
    char state = '?';
    pid_t pgrp = -1;
    if (closeParen != nullptr) {
        sscanf(closeParen + 2, " %c %*d %d", &state, &pgrp);
    }

    bool isForeground = false;
    pid_t termPgrp = tcgetpgrp(STDIN_FILENO);
    if (termPgrp != -1 && pgrp != -1 && termPgrp == pgrp) {
        isForeground = true;
    }

    char stateStr[4];
    snprintf(stateStr, sizeof(stateStr), "%c%s", state, isForeground ? "+" : "");

    char statusPath[64];
    snprintf(statusPath, sizeof(statusPath), "/proc/%d/status", pid);

    char statusBuf[4096];
    long vmSizeKb = -1;
    if (readFileRaw(statusPath, statusBuf, sizeof(statusBuf)) > 0) {
        char* line = strstr(statusBuf, "VmSize:");
        if (line != nullptr) {
            sscanf(line + 7, "%ld", &vmSizeKb);
        }
    }

    char exePath[64];
    snprintf(exePath, sizeof(exePath), "/proc/%d/exe", pid);

    char exeResolved[PATH_MAX];
    ssize_t len2 = readlink(exePath, exeResolved, sizeof(exeResolved) - 1);
    if (len2 != -1) {
        exeResolved[len2] = '\0';
    } else {
        strncpy(exeResolved, "?", sizeof(exeResolved));
    }

    printf("Process Status -- %s\n", stateStr);
    if (vmSizeKb != -1) {
        printf("memory -- %ld {Virtual Memory}\n", vmSizeKb);
    } else {
        printf("memory -- ? {Virtual Memory}\n");
    }
    printf("Executable Path -- %s\n", exeResolved);
#endif
}

// echo
void CommandHandler::echo(const Command& command) {

    bool n_flag = false;   // -n : suppress trailing newline
    bool e_flag = false;   // -e : interpret backslash escapes (\n, \t, etc.)
    int startIdx = 1;

    for (int i = 1; command.argv[i] != nullptr; i++) {
        char* arg = command.argv[i];

        if (strcmp(arg, "-n") == 0) {
            n_flag = true;
            startIdx = i + 1;
        } else if (strcmp(arg, "-e") == 0) {
            e_flag = true;
            startIdx = i + 1;
        } else if (strcmp(arg, "-ne") == 0 || strcmp(arg, "-en") == 0) {
            n_flag = true;
            e_flag = true;
            startIdx = i + 1;
        } else {
            break;
        }
    }

    char outBuf[4096];
    size_t outLen = 0;
    outBuf[0] = '\0';

    for (int i = startIdx; command.argv[i] != nullptr; i++) {
        char* arg = command.argv[i];

        if (i > startIdx && outLen < sizeof(outBuf) - 1) {
            outBuf[outLen++] = ' ';
        }

        for (size_t j = 0; arg[j] != '\0' && outLen < sizeof(outBuf) - 1; j++) {
            if (e_flag && arg[j] == '\\' && arg[j + 1] != '\0') {
                char next = arg[j + 1];
                char resolved = '\0';
                bool handled = true;

                // Handle common escape sequences
                switch (next) {
                    case 'n': resolved = '\n'; break;
                    case 't': resolved = '\t'; break;
                    case '\\': resolved = '\\'; break;
                    case 'r': resolved = '\r'; break;
                    default: handled = false; break;
                }

                // If the escape sequence was recognized, add the resolved character to the output buffer and skip the next character
                if (handled) {
                    outBuf[outLen++] = resolved;
                    j++; // skip the escaped char
                    continue;
                }
            }
            outBuf[outLen++] = arg[j];
        }
    }

    // Add a newline at the end of the output unless -n flag is set, and ensure we don't exceed the buffer size
    if (!n_flag && outLen < sizeof(outBuf) - 1) {
        outBuf[outLen++] = '\n';
    }
    outBuf[outLen] = '\0';

    ssize_t written = 0;
    ssize_t total = 0;
    // Write the output buffer to STDOUT in a loop to handle partial writes
    while (total < (ssize_t)outLen &&
           (written = write(STDOUT_FILENO, outBuf + total, outLen - total)) > 0) {
        total += written;
    }

    if (written < 0) {
        perror("echo: write failed");
    }
}

static bool applyPipelineRedirection(const Command& command) {
    // open input file for reading if given, and redirect stdin to it
    if (command.inputFile != nullptr) {
        int fd = open(command.inputFile, O_RDONLY);
        if (fd == -1) {
            perror(command.inputFile);
            return false;
        }
        // Redirect stdin to the input file
        if (dup2(fd, STDIN_FILENO) == -1) {
            perror("dup2");
            close(fd);
            return false;
        }
        close(fd);
    }

    // open output file for writing if given, and redirect stdout to it
    if (command.outputFile != nullptr) {
        int flags = O_WRONLY | O_CREAT |
            (command.appendOutput ? O_APPEND : O_TRUNC);
        int fd = open(command.outputFile, flags, 0644);
        if (fd == -1) {
            perror(command.outputFile);
            return false;
        }
        // Redirect stdout to the output file
        if (dup2(fd, STDOUT_FILENO) == -1) {
            perror("dup2");
            close(fd);
            return false;
        }
        close(fd);
    }

    // open error file for writing if given, and redirect stderr to it
    if (command.errorFile != nullptr) {
        int fd = open(command.errorFile, O_WRONLY | O_CREAT |
                      (command.appendOutput ? O_APPEND : O_TRUNC), 0644);
        if (fd == -1) {
            perror(command.errorFile);
            return false;
        }
        // Redirect stderr to the error file
        if (dup2(fd, STDERR_FILENO) == -1) {
            perror("dup2");
            close(fd);
            return false;
        }
        close(fd);
    }

    return true;
}

void CommandHandler::executePipeline(Command** commands, int count) {
    if (commands == nullptr || count <= 0) {
        return;
    }

    // Create pipes for inter-process communication
    int (*pipes)[2] = nullptr;
    if (count > 1) {
        pipes = new int[count - 1][2];
        for (int i = 0; i < count - 1; i++) {
            if (pipe(pipes[i]) == -1) {
                perror("pipe");
                for (int j = 0; j < i; j++) {
                    close(pipes[j][0]);
                    close(pipes[j][1]);
                }
                delete[] pipes;
                return;
            }
        }
    }

    // Array to hold child process IDs
    pid_t* pids = new pid_t[count];
    pid_t processGroup = 0;
    int started = 0;

    // Loop through each command in the pipeline and fork a new process
    for (int i = 0; i < count; i++) {
        fflush(nullptr);
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork failed");
            break;
        }
        
        if (pid == 0) {
            // Child process
            if (processGroup == 0) {
                processGroup = getpid();
            }
            // Set the process group for the child process
            if (setpgid(0, processGroup) == -1) {
                perror("setpgid");
                _exit(1);
            }
            // Set the terminal control to the process group of the child
            tcsetpgrp(STDIN_FILENO, processGroup);
            if (i > 0 && dup2(pipes[i - 1][0], STDIN_FILENO) == -1) {
                perror("dup2");
                _exit(1);
            }
            // Redirect stdout to the next pipe if not the last command
            if (i < count - 1 && dup2(pipes[i][1], STDOUT_FILENO) == -1) {
                perror("dup2");
                _exit(1);
            }
            // Close all pipe file descriptors in the child process
            for (int j = 0; j < count - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            // Reset signal handlers to default in the child process
            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            signal(SIGQUIT, SIG_DFL);

            // Apply redirection for input/output/error files if specified in the command
            if (!applyPipelineRedirection(*commands[i])) {
                _exit(1);
            }

            // Check if the command is a built-in command and execute it if so
            if (executeBuiltin(*commands[i])) {
                fflush(nullptr);
                _exit(lastExitStatus);
            }
            
            // Execute external command using execvp
            execvp(commands[i]->name, commands[i]->argv);
            fprintf(stderr, "%s: command not found\n", commands[i]->name);
            _exit(127);
        }

        // Parent process
        if (processGroup == 0) {
            processGroup = pid;
        }
        // Set the process group for the child process in the parent
        setpgid(pid, processGroup);
        // Store the child process ID in the array
        pids[started++] = pid;
    }

    // Close all pipe file descriptors in the parent process
    for (int i = 0; i < count - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
    delete[] pipes;

    // Wait for all child processes to finish and collect their exit statuses
    if (started > 0) {
        // Set the terminal control to the process group of the last child
        tcsetpgrp(STDIN_FILENO, processGroup);
        int status = 0;
        for (int i = 0; i < started; i++) {
            int childStatus;
            // Wait for each child process to finish and collect its exit status
            if (waitpid(pids[i], &childStatus, WUNTRACED) == -1) {
                perror("waitpid");
                continue;
            }
            if (i == started - 1) {
                status = childStatus;
            }
        }
        tcsetpgrp(STDIN_FILENO, getpid());

        if (WIFEXITED(status)) {
            lastExitStatus = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            lastExitStatus = 128 + WTERMSIG(status);
        }
    }

    delete[] pids;
}

// external command execution
void CommandHandler::executeExternal(Command* cmd) {
    pid_t pid = fork();

    if (pid == -1) {
        perror("fork failed");
        return;
    }

    if (pid == 0) {
        // child process
        setpgid(0, 0);

        tcsetpgrp(STDIN_FILENO, getpid());

        // Reset signal handlers to default in the child process
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);

        // Apply redirection for input/output/error files if specified in the command
        if (cmd->outputFile != nullptr) {
            int flags = O_WRONLY | O_CREAT | (cmd->appendOutput ? O_APPEND : O_TRUNC);
            int fd = open(cmd->outputFile, flags, 0644);
            if (fd == -1) {
                perror(cmd->outputFile);
                _exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }

        // Apply redirection for input file if specified in the command
        if (cmd->inputFile != nullptr) {
            int fd = open(cmd->inputFile, O_RDONLY);
            if (fd == -1) {
                perror(cmd->inputFile);
                _exit(1);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }

        // Apply redirection for error file if specified in the command
        if (cmd->errorFile != nullptr) {
            int flags = O_WRONLY | O_CREAT | (cmd->appendOutput ? O_APPEND : O_TRUNC);
            int fd = open(cmd->errorFile, flags, 0644);
            if (fd == -1) {
                perror(cmd->errorFile);
                _exit(1);
            }
            dup2(fd, STDERR_FILENO);
            close(fd);
        }

        execvp(cmd->name, cmd->argv);

        // If execvp returns, it means the command was not found or failed to execute
        fprintf(stderr, "%s: command not found\n", cmd->name);
        _exit(127);
    }

    // parent process
    setpgid(pid, pid);
    tcsetpgrp(STDIN_FILENO, pid);
    
    int status;
    // wait for the child to finish
    waitpid(pid, &status, WUNTRACED);

    // shell takes back control after child is finished.
    tcsetpgrp(STDIN_FILENO, getpid());

    // debug and testing lines
    // if (WIFEXITED(status)) {
    //     lastExitStatus = WEXITSTATUS(status); // exitStatus = 0 if exited normally
    // } else if (WIFSIGNALED(status)) {
    //     printf("Terminated by signal %d\n", WTERMSIG(status)); // exitStatus = 128 + signal number if terminated by signal
    // }
}