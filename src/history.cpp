#include "history.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <cstdio>
#include <unistd.h>

namespace {
const mode_t historyFileMode = 0600;

void historyError(const char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}
}

History::History() : commandCount(0), currentIndex(0), fd(-1) {
    const char *historyFile = getenv("HISFILE");
    if (historyFile == nullptr) {
        historyFile = ".anujsh";
    }

    fd = open(historyFile, O_RDWR | O_CREAT, historyFileMode);
    if (fd == -1) {
        historyError("history: failed to open file");
    }

    char line[1024];
    ssize_t length;
    while ((length = readLine(line, sizeof(line))) >= 0) {
        if (length == 0) {
            continue;
        }
        if (commandCount == maxCommands) {
            free(commands[0]);
            for (int i = 1; i < commandCount; ++i) {
                commands[i - 1] = commands[i];
            }
            --commandCount;
        }
        commands[commandCount] = strdup(line);
        if (commands[commandCount] == nullptr) {
            historyError("history: memory allocation failed");
        }
        ++commandCount;
    }

    currentIndex = commandCount;
    if (ftruncate(fd, 0) == -1 || lseek(fd, 0, SEEK_SET) == -1) {
        historyError("history: failed to update file");
    }
    for (int i = 0; i < commandCount; ++i) {
        write(fd, commands[i], strlen(commands[i]));
        write(fd, "\n", 1);
    }
}

History::~History() {
    for (int i = 0; i < commandCount; ++i) {
        free(commands[i]);
    }
    close(fd);
}

ssize_t History::readLine(char *line, size_t capacity) {
    size_t length = 0;
    char character = '\0';
    bool readCharacter = false;
    while (read(fd, &character, 1) == 1) {
        readCharacter = true;
        if (character == '\n') {
            break;
        }
        if (length + 1 < capacity) {
            line[length++] = character;
        }
    }
    if (!readCharacter) {
        return -1;
    }
    line[length] = '\0';
    return static_cast<ssize_t>(length);
}

void History::add(const char *command) {
    if (command == nullptr || *command == '\0') {
        return;
    }

    if (commandCount == maxCommands) {
        free(commands[0]);
        for (int i = 1; i < commandCount; ++i) {
            commands[i - 1] = commands[i];
        }
        --commandCount;
    }
    commands[commandCount++] = strdup(command);
    currentIndex = commandCount;

    if (ftruncate(fd, 0) == -1 || lseek(fd, 0, SEEK_SET) == -1) {
        historyError("history: failed to update file");
    }
    for (int i = 0; i < commandCount; ++i) {
        write(fd, commands[i], strlen(commands[i]));
        write(fd, "\n", 1);
    }
}

const char *History::previous() {
    if (commandCount == 0 || currentIndex == 0) {
        return commandCount == 0 ? nullptr : commands[0];
    }
    return commands[--currentIndex];
}

const char *History::next() {
    if (commandCount == 0) {
        return nullptr;
    }
    if (currentIndex + 1 < commandCount) {
        return commands[++currentIndex];
    }
    currentIndex = commandCount;
    return "";
}
