#pragma once

#include "command.hpp"

class CommandHandler {
public:
    void execute(const Command& command);

private:
    void cd(const Command& command);
    void ls(const Command& command);
    void pwd(const Command& command);
    void cat(const Command& command);
    void search(const Command& command);
    void grep(const Command& command);
    void history(const Command& command);
    void pinfo(const Command& command);
    void echo(const Command& command);
};