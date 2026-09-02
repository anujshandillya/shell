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

using namespace std;

void CommandHandler::execute(const Command& command) {
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

// search
static bool searchRecursive(const char* dirPath, const char* target) {
    DIR* dir = opendir(dirPath);
    if (dir == nullptr) {
        return false; // can't open this dir (permissions, etc.) — just skip it
    }

    struct dirent* entry;
    bool found = false;

    while (!found && (entry = readdir(dir)) != nullptr) {
        // Skip "." and ".." to avoid infinite recursion
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Match by name, regardless of whether it's a file or directory
        if (strcmp(entry->d_name, target) == 0) {
            found = true;
            break;
        }

        // Build full path for recursion
        char fullPath[PATH_MAX];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", dirPath, entry->d_name);

        struct stat st;
        // Use lstat (not stat) so we don't follow symlinks — avoids infinite
        // loops from symlinked directories that point back up the tree
        if (lstat(fullPath, &st) != 0) {
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
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
        int fd = open(historyFilePath, O_WRONLY | O_TRUNC | O_CREAT, 0644);
        if (fd == -1) {
            perror("history: failed to clear");
            return;
        }
        close(fd);
        printf("History cleared.\n");
        return;
    }

    // ---- Read entire history file into memory ----
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

    // ---- Split into lines, count total first (needed for -n / limit) ----
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
                    write(STDOUT_FILENO, buf + lineStart, i - lineStart);
                    write(STDOUT_FILENO, "\n", 1);
                }
                currentLine++;
            }
            lineStart = i + 1;
        }
    }
}

// pinfo
#ifdef __APPLE__
#include <sys/sysctl.h>
#include <libproc.h>
#endif

void CommandHandler::pinfo(const Command& command) {
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
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, pid };
    struct kinfo_proc kp;
    size_t len = sizeof(kp);

    if (sysctl(mib, 4, &kp, &len, nullptr, 0) != 0 || len == 0) {
        printf("ps: no such process: %d\n", pid);
        return;
    }

    char state = '?';
    switch (kp.kp_proc.p_stat) {
        case SRUN:    state = 'R'; break;
        case SSLEEP:  state = 'S'; break;
        case SSTOP:   state = 'T'; break;
        case SZOMB:   state = 'Z'; break;
        case SIDL:    state = 'I'; break;
        default:      state = '?'; break;
    }

    pid_t pgrp = kp.kp_eproc.e_pgid;
    bool isForeground = false;
    pid_t termPgrp = tcgetpgrp(STDIN_FILENO);
    if (termPgrp != -1 && termPgrp == pgrp) {
        isForeground = true;
    }

    char stateStr[4];
    snprintf(stateStr, sizeof(stateStr), "%c%s", state, isForeground ? "+" : "");

    // ---- 2. Memory via proc_pidinfo ----
    struct proc_taskinfo taskInfo;
    long vmSizeKb = -1;
    if (proc_pidinfo(pid, PROC_PIDTASKINFO, 0, &taskInfo, sizeof(taskInfo)) > 0) {
        vmSizeKb = (long)(taskInfo.pti_virtual_size / 1024); // bytes -> KB
    }

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

    // Parse leading flags only (real echo stops flag-parsing at the first non-flag arg)
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
            break; // first non-flag token: stop parsing flags
        }
    }

    // Build the output string
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

                switch (next) {
                    case 'n': resolved = '\n'; break;
                    case 't': resolved = '\t'; break;
                    case '\\': resolved = '\\'; break;
                    case 'r': resolved = '\r'; break;
                    default: handled = false; break;
                }

                if (handled) {
                    outBuf[outLen++] = resolved;
                    j++; // skip the escaped char
                    continue;
                }
            }
            outBuf[outLen++] = arg[j];
        }
    }

    if (!n_flag && outLen < sizeof(outBuf) - 1) {
        outBuf[outLen++] = '\n';
    }
    outBuf[outLen] = '\0';

    // Write using raw syscall, not stdio
    ssize_t written = 0;
    ssize_t total = 0;
    while (total < (ssize_t)outLen &&
           (written = write(STDOUT_FILENO, outBuf + total, outLen - total)) > 0) {
        total += written;
    }

    if (written < 0) {
        perror("echo: write failed");
    }
}