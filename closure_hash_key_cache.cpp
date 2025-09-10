#include <iostream>
#include <functional>
#include <memory>
#include <unordered_map>
#include <string>

// Closure, создающая уникальный ключ для хранилища на основе строки и возвращающая этот ключ.
std::function<std::string()> makeKeyClosure(const std::string& base) {
    // Храним результат хэширования строки через std::hash, как уникальный идентификатор.
    auto key = std::make_shared<std::string>(std::to_string(std::hash<std::string>{}(base)));
    // Возвращаем closure, которая хранит и отдаёт уникальный ключ.
    return [key]() { return *key; };
}

int main() {
    // Создаём cache для хранения значений по уникальному ключу.
    std::unordered_map<std::string, std::string> myCache;

    // Closure с уникальным ключом для фразы.
    auto keyClosure = makeKeyClosure("example string");
    // Получаем сам ключ (его можно использовать для индексации в cache).
    std::string cacheKey = keyClosure();
    
    // Сохраняем данные в cache по этому ключу.
    myCache[cacheKey] = "Cached value for example string";

    // Проверяем результат:
    std::cout << "Generated key: " << cacheKey << std::endl;
    std::cout << "Value from cache: " << myCache[cacheKey] << std::endl;

    // Для другой строки будет другой уникальный ключ:
    auto otherClosure = makeKeyClosure("another string");
    std::string otherKey = otherClosure();
    myCache[otherKey] = "Cached value for another string";

    std::cout << "Other key: " << otherKey << std::endl;
    std::cout << "Other value: " << myCache[otherKey] << std::endl;

    return 0;
}
