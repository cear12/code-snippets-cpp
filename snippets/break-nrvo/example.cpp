// A pattern that has been observed to defeat NRVO on some compilers
// (notably older MSVC): an earlier return of a *different* temporary, even
// in dead code, can make the optimizer give up on eliding the later named
// return. Still fully correct (a move, not a bug) on every compiler --
// this file exists to document the gotcha, not to recommend the pattern.
#include <iostream>
#include <string>

std::string MaybeBreaksNrvo() {
    if (false) {
        return std::string("never get here"); // an earlier, different return value
    }

    std::string s = "Hi!";
    return s; // NRVO-eligible in principle; some compilers stop optimizing
              // this once they've seen the unreachable return above
}

int main() {
    std::cout << MaybeBreaksNrvo() << "\n";
    return 0;
}
