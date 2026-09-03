// boost::copy + boost::adaptors::filtered idiom: composes a filter
// directly into a copy call via Boost.Range's pipe-style adaptors.
// Requires Boost (not compiled locally in this portfolio -- see
// README.md; verified by CI instead).
#include <algorithm>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include <boost/range/adaptor/filtered.hpp>
#include <boost/range/algorithm/copy.hpp>

struct Person {
  std::string name;
  bool isFemale;
};

int main() {
  std::vector<Person> people{
      {"Alice", true}, {"Bob", false}, {"Carol", true}, {"Dave", false}};

  auto isFemale = [](const Person &p) { return p.isFemale; };

  std::cout << "Female names: ";
  boost::copy(
      people | boost::adaptors::filtered(isFemale) |
          boost::adaptors::transformed([](const Person &p) { return p.name; }),
      std::ostream_iterator<std::string>(std::cout, " "));
  std::cout << "\n";

  return 0;
}
