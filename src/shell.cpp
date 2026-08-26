#include "shell.hpp"
#include "parser.hpp"
#include "history.hpp"

#include <iostream>
#include <string>
#include <csignal>
#include <unistd.h>
#include <cstdlib>

using namespace std;

Shell::Shell() : is_running(true) {
    cout << "Running...\n";
    signal(SIGINT, signal_handler);
    const char* user = getenv("USER");
    username = user;
    
    if(gethostname(Shell::host, sizeof(Shell::host)) == 0) {
        return;
    }
}

Shell::~Shell() = default;

void Shell::Print() {
    cout << "<" << Shell::username << "@" << Shell::host << ":~> ";
    cout.flush();    
}

void Shell::signal_handler(int signum) {
    cout << "Closing Shell...";
    cout.flush();
    // Shell::is_running = false;
}

void Shell::process_input(const string& input) {
    Shell::history.add(input);
    if(input == "exit") {
        Shell::is_running = false;
        return;
    }
}

void Shell::run() {
    while (is_running) {
        Print();
        string input;

        if (!getline(cin, input)) {
            break;
        }

        cout << "You entered: " << input << endl;
        Shell::process_input(input);
    }
}