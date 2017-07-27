#include "testbussecondary.h"
#include "uptane/uptanerepository.h"

namespace Uptane {
TestBusSecondary::TestBusSecondary(Uptane::Repository *primary) : primary_(primary) {}
std::string TestBusSecondary::getImage(const Target &target) { return primary_->director.downloadTarget(target); }
}
