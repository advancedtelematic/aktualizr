#include "p11engine.h"

P11Engine* P11EngineGuard::instance = nullptr;
int P11EngineGuard::ref_counter = 0;

P11ContextWrapper::P11ContextWrapper(const boost::filesystem::path& module) : ctx(nullptr) {
  (void)module;
  throw std::runtime_error("Aktualizr was built without PKCS#11");
}

P11ContextWrapper::~P11ContextWrapper() = default;

P11SlotsWrapper::P11SlotsWrapper(PKCS11_ctx_st* ctx_in) : ctx(nullptr), wslots_(nullptr), nslots(0) {
  (void)ctx_in;
  throw std::runtime_error("Aktualizr was built without PKCS#11");
}

P11SlotsWrapper::~P11SlotsWrapper() = default;
