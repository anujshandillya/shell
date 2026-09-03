#pragma once

#include "command.hpp"

class CommandHandler {
public:
    void execute(const Command& command);
    static int lastExitStatus; 

private:
    void cd(const Command& command); // Done
    void ls(const Command& command); // Done
    void pwd(const Command& command); // Done
    void search(const Command& command); // Done
    void history(const Command& command); // Done
    void pinfo(const Command& command); // Done
    void echo(const Command& command); // Done
    void executeExternal(Command* cmd); // Done
};