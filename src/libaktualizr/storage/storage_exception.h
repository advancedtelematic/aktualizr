#ifndef STORAGE_EXCEPTION_H_
#define STORAGE_EXCEPTION_H_

class StorageException : public std::runtime_error {
 public:
  StorageException(const std::string& what) : std::runtime_error(what) {}
  ~StorageException() noexcept override = default;
};

#endif  // STORAGE_EXCEPTION_H_
