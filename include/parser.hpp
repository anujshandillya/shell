#pragma once

#include "command.hpp"

#include <iostream>

using namespace std;

class Parser {
public:
    Command **parse(char *input);
};