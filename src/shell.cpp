#include "shell.hpp"
#include "parser.hpp"
#include "history.hpp"

#include <iostream>
#include <string>
#include <csignal>
#include <termios.h>
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

struct termios origTermios;

void Shell::enableRawMode() {
    tcgetattr(STDIN_FILENO, &origTermios);
    struct termios raw = origTermios;
    raw.c_lflag &= ~(ICANON | ECHO); // disable line buffering AND default echo
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void Shell::disableRawMode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &origTermios);
}

void Shell::readLine(char* buf, size_t bufSize) {
    size_t len = 0;        // total chars in buffer
    size_t cursorPos = 0;  // where the cursor currently is
    buf[0] = '\0';

    while (true) {
        char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n <= 0) {
            buf[len] = '\0';
            return;
        }

        if (c == '\t') {
            // Handle tab autocompletion
            // handleTabCompletion(buf, len, bufSize);
            continue;
        }

        if (c == '\n' || c == '\r') {
            write(STDOUT_FILENO, "\n", 1);
            break;
        }

        // ---- Backspace: delete char BEFORE cursor ----
        if (c == 127 || c == '\b') {
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

        cout << "You entered: " << input << endl;
        is_processing_command = true;
        Shell::process_input(input);
        is_processing_command = false;
    }

    disableRawMode();
}