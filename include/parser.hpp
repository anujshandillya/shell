#include "command.hpp"

#include <iostream>
#include <vector>
#include <string>

using namespace std;

class Parser {
public:
    vector<Command> parse(const string& input);
};