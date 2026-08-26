#include <iostream>
#include <vector>
#include <string>

using namespace std;

class History {
public:
    void add(const string& command);

    const string& previous();
    const string& next();

    void print();

private:
    vector<string> commands;
    int currentIndex;
};