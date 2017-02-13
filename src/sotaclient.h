#ifndef SOTACLIENT_H_
#define SOTACLIENT_H_

#include "types.h"

class SotaClient{
  public:
    virtual void startDownload(const data::UpdateRequestId &update_id) = 0;
    virtual void sendUpdateReport(data::UpdateReport update_report) = 0;
};

#endif //SOTACLIENT_H_