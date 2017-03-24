#ifndef OSTREE_H_
#define OSTREE_H_

#include <string>
#include "types.h"



#include "ostree-1/ostree.h"

#include <glib/gi18n.h>


typedef std::pair<data::UpdateResultCode, std::string> InstallOutcome;
static const std::string NEW_PACKAGE = "/tmp/sota-package";

class OstreePackage{
    public:
        OstreePackage(const std::string &ecu_serial_in, const std::string &ref_name_in, const std::string &commit_in, const std::string &desc_in, const std::string &treehub_in);
        std::string ecu_serial;
        std::string ref_name;
        std::string commit;
        std::string description;
        std::string pull_uri;
        InstallOutcome install(const data::PackageManagerCredentials &cred);
};


class OstreeBranch {
    
};



#endif  // OSTREE_H_