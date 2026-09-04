#include "history.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <unistd.h>

namespace {
constexpr mode_t historyFileMode = 0600;
}

History::History() {
    const char *historyFile = std::getenv("HISFILE");
    if (historyFile == nullptr) {
        historyFile = ".anujsh";
    }

    fd = open(historyFile, O_RDWR | O_CREAT, historyFileMode);
    if (fd == -1) {
        throw std::runtime_error(
            std::string("history: failed to open file: ") + std::strerror(errno));
    }

    std::string contents;
    char buffer[4096];
    ssize_t bytesRead;
    while ((bytesRead = read(fd, buffer, sizeof(buffer))) > 0) {
        contents.append(buffer, static_cast<std::size_t>(bytesRead));
    }
    if (bytesRead == -1) {
        const int error = errno;
        close(fd);
        throw std::runtime_error(
            std::string("history: failed to read file: ") + std::strerror(error));
    }

    std::size_t start = 0;
    while (start < contents.size()) {
        const std::size_t end = contents.find('\n', start);
        const std::size_t length =
            end == std::string::npos ? contents.size() - start : end - start;
        if (length > 0) {
            commands.emplace_back(contents, start, length);
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }

    if (commands.size() > maxCommands) {
        commands.erase(commands.begin(),
                       commands.end() - static_cast<std::ptrdiff_t>(maxCommands));
    }
    currentIndex = commands.size();

    if (ftruncate(fd, 0) == -1 || lseek(fd, 0, SEEK_SET) == -1) {
        const int error = errno;
        close(fd);
        throw std::runtime_error(
            std::string("history: failed to update file: ") + std::strerror(error));
    }
    for (const std::string &entry : commands) {
        const std::string line = entry + '\n';
        if (write(fd, line.data(), line.size()) !=
            static_cast<ssize_t>(line.size())) {
            const int error = errno;
            close(fd);
            throw std::runtime_error(
                std::string("history: failed to update file: ") + std::strerror(error));
        }
    }
}

History::~History() {
    close(fd);
}

void History::add(const char *command) {
    if (command == nullptr || *command == '\0') {
        return;
    }

    commands.emplace_back(command);
    if (commands.size() > maxCommands) {
        commands.erase(commands.begin());
    }
    currentIndex = commands.size();

    if (ftruncate(fd, 0) == -1 || lseek(fd, 0, SEEK_SET) == -1) {
        throw std::runtime_error(
            std::string("history: failed to update file: ") + std::strerror(errno));
    }
    for (const std::string &entry : commands) {
        const std::string line = entry + '\n';
        if (write(fd, line.data(), line.size()) !=
            static_cast<ssize_t>(line.size())) {
            throw std::runtime_error(
                std::string("history: failed to update file: ") + std::strerror(errno));
        }
    }
}