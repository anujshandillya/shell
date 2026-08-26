#include "parser.hpp"
#include "command_handler.hpp"
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
    
    void Print();
    void process_input(const string& input);
    static void signal_handler(int signum);

    Parser parser;
    CommandHandler handler;
    History history;
    // Terminal terminal;
};