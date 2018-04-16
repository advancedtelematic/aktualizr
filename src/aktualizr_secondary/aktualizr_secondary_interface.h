#ifndef AKTUALIZR_SECONDARY_INTERFACE_H_
#define AKTUALIZR_SECONDARY_INTERFACE_H_

class AktualizrSecondaryInterface {
 public:
  virtual ~AktualizrSecondaryInterface() {}
  virtual void run() = 0;
  virtual void stop() = 0;
};

#endif  // AKTUALIZR_SECONDARY_INTERFACE_H_
