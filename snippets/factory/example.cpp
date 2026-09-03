// Self-registering factory idiom: breaks a core-module -> plugin-module
// dependency by having each plugin register itself into core's factory
// at static-initialization time, so core never names a concrete plugin
// class. See README.md for the multi-project layout this simulates.
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>

// ============================================================
// "core" project: knows only the abstraction, never a concrete provider.
// ============================================================
namespace core {

class ICryptoProvider {
public:
  virtual ~ICryptoProvider() = default;
  virtual void DoWork() = 0;
};

class CryptoProviderFactory {
public:
  using Creator = std::function<std::unique_ptr<ICryptoProvider>()>;

  static CryptoProviderFactory &instance() {
    static CryptoProviderFactory factory;
    return factory;
  }

  void registerType(const std::string &id, Creator creator) {
    creators_[id] = std::move(creator);
  }

  std::unique_ptr<ICryptoProvider> Create(const std::string &id) const {
    auto it = creators_.find(id);
    if (it == creators_.end()) {
      throw std::runtime_error("Unknown crypto provider id: " + id);
    }
    return it->second();
  }

private:
  std::unordered_map<std::string, Creator> creators_;
};

} // namespace core

// Self-registration macro: a provider module drops one line
// (REGISTER_CLASS(Derived, Base);) and core's factory learns about it at
// static-init time, with zero changes to core itself.
#define REGISTER_CLASS_CONCAT_INNER(a, b) a##b
#define REGISTER_CLASS_CONCAT(a, b) REGISTER_CLASS_CONCAT_INNER(a, b)

// DerivedClass may be namespace-qualified (e.g. wincrypt::WincryptProvider),
// so its name can't be pasted into an identifier -- the registrar struct
// and instance instead get a __LINE__-derived unique name, and the
// registration id is passed explicitly rather than stringized from
// DerivedClass.
#define REGISTER_CLASS(DerivedClass, BaseClass, IdString)                      \
  static_assert(std::is_base_of_v<BaseClass, DerivedClass>,                    \
                #DerivedClass " must inherit from " #BaseClass);               \
  namespace {                                                                  \
  struct REGISTER_CLASS_CONCAT(Registrar_, __LINE__) {                         \
    REGISTER_CLASS_CONCAT(Registrar_, __LINE__)() {                            \
      core::CryptoProviderFactory::instance().registerType(                    \
          IdString, [] { return std::make_unique<DerivedClass>(); });          \
    }                                                                          \
  } REGISTER_CLASS_CONCAT(registrar_instance_, __LINE__);                      \
  }

// ============================================================
// "wincrypt" project: depends on core, core does not depend on it.
// ============================================================
namespace wincrypt {

class WincryptProvider : public core::ICryptoProvider {
public:
  void DoWork() override {
    std::cout << "WincryptProvider: signing via Windows CryptoAPI\n";
  }
};

} // namespace wincrypt

REGISTER_CLASS(wincrypt::WincryptProvider, core::ICryptoProvider,
               "WincryptProvider");

// ============================================================
// "cryptoki" project: also depends on core, and not on wincrypt either.
// ============================================================
namespace cryptoki {

class CryptokiProvider : public core::ICryptoProvider {
public:
  void DoWork() override {
    std::cout << "CryptokiProvider: signing via a PKCS#11 token\n";
  }
};

} // namespace cryptoki

REGISTER_CLASS(cryptoki::CryptokiProvider, core::ICryptoProvider,
               "CryptokiProvider");

// ============================================================
// Application: only ever names the id strings, never the concrete types.
// ============================================================
int main() {
  for (const auto &id : {"WincryptProvider", "CryptokiProvider"}) {
    try {
      auto provider = core::CryptoProviderFactory::instance().Create(id);
      provider->DoWork();
    } catch (const std::exception &e) {
      std::cerr << "Error creating '" << id << "': " << e.what() << "\n";
    }
  }

  // Unknown id: exercises the error path.
  try {
    core::CryptoProviderFactory::instance().Create("NonexistentProvider");
  } catch (const std::exception &e) {
    std::cout << "Expected failure: " << e.what() << "\n";
  }

  return 0;
}
