#pragma once

#include "command.hpp"

class CommandHandler {
public:
    void execute(const Command& command);
    void executePipeline(Command** commands, int count);
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
    bool executeBuiltin(const Command& command);
};