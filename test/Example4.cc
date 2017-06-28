#include "Format_pretty.h"
#include "Format_string.h" // TreatAsString<std::string> => true
#include <map>
#include <string>
#include <vector>

int main()
{
    std::vector<int> vec = {1, 2, 3, 4, 5};

    fmtxx::format(stdout, "{}\n", fmtxx::pretty(vec));
        // [1, 2, 3, 4, 5]

    std::map<std::string, int> map = {
        {"eins", 1},
        {"zwei", 2},
        {"drei", 3},
    };

    fmtxx::format(stdout, "{}\n", fmtxx::pretty(map));
        // [{"drei", 3}, {"eins", 1}, {"zwei", 2}]
}
