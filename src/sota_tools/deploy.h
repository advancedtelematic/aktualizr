#ifndef SOTA_CLIENT_TOOLS_DEPLOY_H_
#define SOTA_CLIENT_TOOLS_DEPLOY_H_

#include <string>

bool copy_repo(const std::string &cacerts, const std::string &src, const std::string &dst, const std::string &ref, bool dryrun=false);
void sign_repo(const std::string &credentials);


#endif
