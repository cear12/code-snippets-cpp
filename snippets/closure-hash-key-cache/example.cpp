// Closure as a Stable Cache Key Generator: makeKeyClosure computes a hash
// once and captures it by shared_ptr, so every call to the returned
// closure hands back the identical, already-computed key.
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>

std::function<std::string()> makeKeyClosure(const std::string& base) {
    auto key = std::make_shared<std::string>(std::to_string(std::hash<std::string>{}(base)));
    return [key]() { return *key; };
}

int main() {
    std::unordered_map<std::string, std::string> cache;

    auto keyClosure = makeKeyClosure("example string");
    std::string cacheKey = keyClosure();
    cache[cacheKey] = "Cached value for example string";

    std::cout << "Generated key: " << cacheKey << "\n";
    std::cout << "Value from cache: " << cache[cacheKey] << "\n";

    auto otherClosure = makeKeyClosure("another string");
    std::string otherKey = otherClosure();
    cache[otherKey] = "Cached value for another string";

    std::cout << "Other key: " << otherKey << "\n";
    std::cout << "Other value: " << cache[otherKey] << "\n";

    return 0;
}
