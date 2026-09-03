#include "history.hpp"

#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

using namespace std;

History::History() {
    char *HISFILE = getenv("HISFILE");
    if (HISFILE == nullptr) {
        HISFILE = ".anujsh";
    }
    int file = open(HISFILE, O_APPEND | O_CREAT | O_RDWR, 0600);
    History::fd = file;
}
History::~History() {
    close(History::fd);
}

void History::add(const char *command) {
    int commandSize = strlen(command);
    char buffer[commandSize+1];

    for(int i = 0; i < commandSize; i++) {
        buffer[i] = command[i];
    }
    buffer[commandSize] = '\n';

    write(History::fd, buffer, commandSize + 1);
}