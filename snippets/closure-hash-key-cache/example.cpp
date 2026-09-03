// Closure as a Stable Cache Key Generator: makeKeyClosure computes a hash
// once and captures it by shared_ptr, so every call to the returned
// closure hands back the identical, already-computed key.
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>

std::function<std::string()> MakeKeyClosure(const std::string& base) {
    auto key = std::make_shared<std::string>(std::to_string(std::hash<std::string>{}(base)));
    return [key]() { return *key; };
}

int main() {
    std::unordered_map<std::string, std::string> cache;

    auto key_closure = MakeKeyClosure("example string");
    std::string cache_key = key_closure();
    cache[cache_key] = "Cached value for example string";

    std::cout << "Generated key: " << cache_key << "\n";
    std::cout << "Value from cache: " << cache[cache_key] << "\n";

    auto other_closure = MakeKeyClosure("another string");
    std::string other_key = other_closure();
    cache[other_key] = "Cached value for another string";

    std::cout << "Other key: " << other_key << "\n";
    std::cout << "Other value: " << cache[other_key] << "\n";

    return 0;
}
