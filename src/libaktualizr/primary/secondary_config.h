#ifndef PRIMARY_SECONDARY_CONFIG_H_
#define PRIMARY_SECONDARY_CONFIG_H_

namespace Primary {

class SecondaryConfig {
 public:
  explicit SecondaryConfig(const char* type) : type_(type) {}
  virtual const char* type() const { return type_; }
  virtual ~SecondaryConfig() = default;

 private:
  const char* const type_;
};

}  // namespace Primary

#endif  // PRIMARY_SECONDARY_CONFIG_H_
