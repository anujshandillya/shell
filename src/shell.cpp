#include "shell.hpp"
#include "parser.hpp"
#include "history.hpp"

#include <iostream>
#include <string>
#include <csignal>
#include <termios.h>
#include <unistd.h>
#include <cstdlib>
#include <dirent.h>
#include <sys/stat.h>
#include <cstring>

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
    signal(SIGINT, signal_handler); // Handle Ctrl+C

    signal(SIGTTOU, SIG_IGN); // Ignore SIGTTOU to prevent the shell from being stopped when it tries to write to the terminal while in the background
    signal(SIGTTIN, SIG_IGN); // Ignore SIGTTIN to prevent the shell from being stopped when it tries to read from the terminal while in the background

    setpgid(0, 0);
    tcsetpgrp(STDIN_FILENO, getpid());

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

    setenv("HISFILE", "/Users/anujsharma/Developer/shell/.anujsh", 1);

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

    for (int i = 0; parsedCommands[i] != nullptr;) {
        int pipelineLength = 1;
        while (parsedCommands[i + pipelineLength - 1]->pipeToNext &&
               parsedCommands[i + pipelineLength] != nullptr) {
            pipelineLength++;
        }

        if (pipelineLength == 1) {
            handler->execute(*parsedCommands[i]);
        } else {
            handler->executePipeline(&parsedCommands[i], pipelineLength);
        }
        i += pipelineLength;
    }

    // freeCommands(parsedCommands);

    // Add the command to history after processing it
    Shell::history.add(inputCopy);

    bool shouldExit = (strcmp(input, "exit") == 0);

    delete[] parsedCommands; // free array of pointers
    free(inputCopy);

    if (shouldExit) {
        Shell::is_running = false;
    }
}

struct termios origTermios;

namespace {
bool isDirectory(const char *path) {
    struct stat info{};
    return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

struct CompletionMatches {
    char values[128][256];
    int count;
};

// add a completion match to the list if it's not already present
void addCompletion(CompletionMatches *matches, const char *name) {
    // check if same name already exists in matches
    for (int i = 0; i < matches->count; ++i) {
        if (strcmp(matches->values[i], name) == 0) {
            return;
        }
    }
    // matches do not exceed 128 entries, and each entry is limited to 255 characters
    if (matches->count < 128) {
        strncpy(matches->values[matches->count], name, 255);
        matches->values[matches->count][255] = '\0';
        ++matches->count;
    }
}

void scanDirectory(const char *path, const char *prefix, bool commandPosition,
                   bool hasSlash, CompletionMatches *matches) {
    DIR *dir = opendir(path);
    if (dir == nullptr) {
        return;
    }
    struct dirent *entry; // pointer to directory entry
    while ((entry = readdir(dir)) != nullptr) {
        const char *name = entry->d_name;
        // ignore if the name doesn't start with the prefix.
        if (strncmp(name, prefix, strlen(prefix)) != 0 ||
            (name[0] == '.' && prefix[0] == '\0')) {
            continue;
        }

        char fullPath[PATH_MAX];
        // Build the full path to the entry for checking if it's executable
        snprintf(fullPath, sizeof(fullPath), "%s/%s", path, name);
        if (!commandPosition || hasSlash) {
            addCompletion(matches, name); // If not in command position or has a slash, add all matches
        } else if (access(fullPath, X_OK) == 0 && !isDirectory(fullPath)) {
            addCompletion(matches, name); // If in command position, only add executable files (not directories)
        }
    }
    closedir(dir);
}

CompletionMatches completionMatches(const char *buffer, size_t cursorPos) {
    CompletionMatches matches{}; // empty structure
    size_t tokenStart = cursorPos;

    // completion for only the last token in the buffer, so find the start of that token
    while (tokenStart > 0 && buffer[tokenStart - 1] != ' ' &&
           buffer[tokenStart - 1] != '\t') {
        --tokenStart;
    }

    char token[PATH_MAX];
    // calculate the length of the token and copy it into a separate buffer
    size_t tokenLength = cursorPos - tokenStart;
    if (tokenLength >= sizeof(token)) {
        tokenLength = sizeof(token) - 1;
    }
    memcpy(token, buffer + tokenStart, tokenLength);
    token[tokenLength] = '\0';

    char directory[PATH_MAX] = ".";
    char prefix[PATH_MAX];
    const char *slash = strrchr(token, '/');
    bool hasSlash = slash != nullptr;
    if (hasSlash) {
        // If the token contains a slash, separate the directory and prefix for scanning
        size_t directoryLength = slash - token + 1;
        if (directoryLength >= sizeof(directory)) {
            directoryLength = sizeof(directory) - 1;
        }
        memcpy(directory, token, directoryLength);
        directory[directoryLength] = '\0';
        strcpy(prefix, slash + 1);
    } else {
        // If the token does not contain a slash, use the entire token as the prefix
        strcpy(prefix, token);
    }
    // If the token is at the start of the buffer and does not contain a slash, scan the directories in PATH for command completions
    if (tokenStart == 0 && !hasSlash) {
        const char *path = getenv("PATH");
        if (path != nullptr) {
            const char *start = path;
            const char *end;
            char pathEntry[PATH_MAX];
            while (*start != '\0') {
                end = strchr(start, ':');
                size_t length = end == nullptr ? strlen(start) : (size_t)(end - start);
                if (length == 0) {
                    strcpy(pathEntry, ".");
                } else {
                    if (length >= sizeof(pathEntry)) length = sizeof(pathEntry) - 1;
                    memcpy(pathEntry, start, length);
                    pathEntry[length] = '\0';
                }
                scanDirectory(pathEntry, prefix, true, false, &matches);
                if (end == nullptr) break;
                start = end + 1;
            }
        }
    } else {
        scanDirectory(directory, prefix, false, hasSlash, &matches);
    }
    return matches;
}

void redrawLine(char *buffer, size_t &length, size_t &cursorPos,
                const char *replacement, size_t replacementCursorPos) {
    // Move the cursor back to the start of the line and clear the line
    for (size_t i = 0; i < cursorPos; ++i) {
        write(STDOUT_FILENO, "\b", 1);
    }
    // Clear the line by overwriting with spaces and moving back again
    for (size_t i = 0; i < length; ++i) {
        write(STDOUT_FILENO, " ", 1);
    }
    // Move the cursor back to the start of the line again
    for (size_t i = 0; i < length; ++i) {
        write(STDOUT_FILENO, "\b", 1);
    }

    const size_t replacementLength = strlen(replacement);
    // Copy the replacement string into the buffer and update length and cursor position
    memcpy(buffer, replacement, replacementLength + 1);
    length = replacementLength;
    cursorPos = replacementCursorPos;
    // Write the updated buffer to STDOUT and move the cursor to the correct position
    write(STDOUT_FILENO, buffer, length);
    for (size_t i = length; i > cursorPos; --i) {
        write(STDOUT_FILENO, "\b", 1);
    }
}

}

// Enable raw mode for terminal input, disabling line buffering and echoing
void Shell::enableRawMode() {
    tcgetattr(STDIN_FILENO, &origTermios);
    struct termios raw = origTermios;
    raw.c_lflag &= ~(ICANON | ECHO); // disable line buffering AND default echo
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

// Restore the original terminal settings when exiting raw mode
void Shell::disableRawMode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &origTermios);
}

void Shell::readLine(char* buf, size_t bufSize) {
    size_t len = 0;        // total chars in buffer
    size_t cursorPos = 0;  // where the cursor currently is
    char savedLine[1024] = "";
    bool browsingHistory = false;
    buf[0] = '\0';

    while (true) {
        char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n <= 0) {
            buf[len] = '\0';
            return;
        }

        // Ctrl+D signals EOF when there is no pending input.
        if (c == 4) {
            if (len == 0) {
                write(STDOUT_FILENO, "\n", 1);
                is_running = false;
                return;
            }
            continue;
        }

        // handle tab keystroke for autocompletion/suggestions
        if (c == '\t') {
            CompletionMatches matches = completionMatches(buf, cursorPos);
            if (matches.count == 0) {
                write(STDOUT_FILENO, "\a", 1);
                continue;
            }

            size_t tokenStart = cursorPos;
            while (tokenStart > 0 && buf[tokenStart - 1] != ' ' &&
                   buf[tokenStart - 1] != '\t') {
                --tokenStart;
            }
            if (matches.count > 1) {
                // Keep the current input line in place while showing the matches.
                write(STDOUT_FILENO, "\033[s", 3);
                write(STDOUT_FILENO, "\r\n", 2);
                for (int i = 0; i < matches.count; ++i) {
                    write(STDOUT_FILENO, matches.values[i],
                          strlen(matches.values[i]));
                    write(STDOUT_FILENO, "  ", 2);
                }
                write(STDOUT_FILENO, "\r\n", 2);
                write(STDOUT_FILENO, "\033[u", 3);
                continue;
            }

            char completed[1024];
            strcpy(completed, buf);
            completed[tokenStart] = '\0';
            strcat(completed, matches.values[0]);
            char candidate[PATH_MAX];
            const char *slash = strrchr(buf + tokenStart, '/');
            if (slash != nullptr) {
                size_t directoryLength = slash - (buf + tokenStart) + 1;
                memcpy(candidate, buf + tokenStart, directoryLength);
                candidate[directoryLength] = '\0';
                strcat(candidate, matches.values[0]);
            } else {
                strcpy(candidate, matches.values[0]);
            }
            const bool candidateIsDirectory = isDirectory(candidate);
            if (candidateIsDirectory) {
                strcat(completed, "/");
            } else if (tokenStart == 0) {
                strcat(completed, " ");
            }
            strcat(completed, buf + cursorPos);
            if (strlen(completed) < bufSize) {
                const size_t insertedLength =
                    strlen(matches.values[0]) +
                    (candidateIsDirectory || tokenStart == 0 ? 1 : 0);
                redrawLine(buf, len, cursorPos, completed,
                           tokenStart + insertedLength);
            }
            continue;
        }

        // execute command  on hitting Enter
        if (c == '\n' || c == '\r') {
            write(STDOUT_FILENO, "\n", 1);
            break;
        }

        // ---- Backspace: delete char BEFORE cursor ----
        if (c == 127 || c == '\b') {
            browsingHistory = false;
            if (cursorPos > 0) {
                // shift everything right of cursor one slot left
                memmove(buf + cursorPos - 1, buf + cursorPos, len - cursorPos);
                len--;
                cursorPos--;
                buf[len] = '\0';

                // redraw: move cursor back, reprint tail, erase leftover char, reposition
                write(STDOUT_FILENO, "\b", 1);
                write(STDOUT_FILENO, buf + cursorPos, len - cursorPos);
                write(STDOUT_FILENO, " ", 1); // clear the now-stale trailing char
                // move cursor back to correct position (tail length + 1 for the space)
                for (size_t i = 0; i < len - cursorPos + 1; i++) {
                    write(STDOUT_FILENO, "\b", 1);
                }
            }
            continue;
        }

        // ---- Escape sequences: arrow keys, Delete key ----
        if (c == 27) { // ESC
            char seq[2];
            if (read(STDIN_FILENO, &seq[0], 1) != 1) continue;
            if (read(STDIN_FILENO, &seq[1], 1) != 1) continue;

            if (seq[0] == '[') {
                if (seq[1] == 'A') { // Up arrow
                    if (!browsingHistory) {
                        memcpy(savedLine, buf, len);
                        savedLine[len] = '\0';
                        browsingHistory = true;
                    }
                    const char *command = history.previous();
                    if (command != nullptr) {
                        redrawLine(buf, len, cursorPos, command, strlen(command));
                    }
                    continue;
                }
                if (seq[1] == 'B') { // Down arrow
                    const char *command = history.next();
                    if (command != nullptr) {
                        if (browsingHistory && command[0] == '\0') {
                            redrawLine(buf, len, cursorPos, savedLine,
                                       strlen(savedLine));
                            browsingHistory = false;
                        } else {
                            redrawLine(buf, len, cursorPos, command, strlen(command));
                        }
                    }
                    continue;
                }
                if (seq[1] == 'C') { // Right arrow
                    if (cursorPos < len) {
                        write(STDOUT_FILENO, buf + cursorPos, 1);
                        cursorPos++;
                    }
                    continue;
                }
                if (seq[1] == 'D') { // Left arrow
                    if (cursorPos > 0) {
                        cursorPos--;
                        write(STDOUT_FILENO, "\b", 1);
                    }
                    continue;
                }
                if (seq[1] == '3') { // Delete key: ESC [ 3 ~
                    char tilde;
                    if (read(STDIN_FILENO, &tilde, 1) == 1 && tilde == '~') {
                        if (cursorPos < len) {
                            // shift everything right of cursor one slot left
                            memmove(buf + cursorPos, buf + cursorPos + 1, len - cursorPos - 1);
                            len--;
                            buf[len] = '\0';

                            // redraw tail, clear stale trailing char, move cursor back
                            write(STDOUT_FILENO, buf + cursorPos, len - cursorPos);
                            write(STDOUT_FILENO, " ", 1);
                            for (size_t i = 0; i < len - cursorPos + 1; i++) {
                                write(STDOUT_FILENO, "\b", 1);
                            }
                        }
                    }
                    continue;
                }
            }
            continue; // unrecognized escape sequence
        }

        // ---- Normal printable character: insert at cursor ----
        browsingHistory = false;
        if (len < bufSize - 1) {
            // shift everything right of cursor one slot right to make room
            memmove(buf + cursorPos + 1, buf + cursorPos, len - cursorPos);
            buf[cursorPos] = c;
            len++;
            cursorPos++;
            buf[len] = '\0';

            // redraw from cursor's old position onward, then reposition cursor
            write(STDOUT_FILENO, buf + cursorPos - 1, len - (cursorPos - 1));
            for (size_t i = 0; i < len - cursorPos; i++) {
                write(STDOUT_FILENO, "\b", 1);
            }
        }
    }

    buf[len] = '\0';
}
void Shell::run() {
    enableRawMode();

    while (is_running) {
        Print();
        char input[1024];

        readLine(input, sizeof(input));

        if (!is_running) {
            break;
        }

        // cout << "You entered: " << input << endl;
        is_processing_command = true;
        Shell::process_input(input);
        is_processing_command = false;
    }

    disableRawMode();
}