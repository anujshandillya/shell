#pragma once

#include "parser.hpp"
// #include "command_handler.hpp"
#include "history.hpp"
// #include "terminal.hpp"

#include <iostream>
#include <string>

using namespace std;

class Shell {
public:
    Shell();
    void run();
    ~Shell();
private:
    atomic<bool> is_running;
    string username;
    char host[_POSIX_HOST_NAME_MAX];
    string home_directory;
    
    void Print();
    void process_input(char *input);
    static void signal_handler(int signum);

    Parser *parser;
    // CommandHandler handler;
    History history;
    // Terminal terminal;
};