// Command-Line Argument Parsing idiom: parsing argv directly (C style)
// versus wrapping it in a vector<string_view> up front for the rest of
// the program to use.
#include <cstring>
#include <iostream>
#include <string_view>
#include <vector>

void ProcessFile(const std::string_view& filename) {
    std::cout << "processing file: " << filename << "\n";
}

int main(int argc, char* argv[]) {
    // --- Style 1: parse argv directly with strcmp.
    bool verbose = false;
    bool test = false;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "-v") == 0) verbose = true;
        if (std::strcmp(argv[i], "-t") == 0) test = true;
    }
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] != '-') ProcessFile(argv[i]);
    }
    std::cout << "verbose=" << std::boolalpha << verbose << " test=" << test << "\n";

    // --- Style 2: wrap argv in a vector<string_view> up front.
    std::vector<std::string_view> args{argv, argv + argc};
    std::cout << "all args:\n";
    for (auto arg : args) {
        std::cout << "  " << arg << "\n";
    }

    return 0;
}
