#include "ostree.h"
#include <stdio.h>


OstreePackage::OstreePackage(const std::string &ecu_serial_in, const std::string &ref_name_in, const std::string &commit_in,
                                const std::string &desc_in, const std::string &treehub_in):ecu_serial(ecu_serial_in), ref_name(ref_name_in),
                                commit(commit_in), description(desc_in), pull_uri(treehub_in){
}

InstallOutcome OstreePackage::install(const data::PackageManagerCredentials & cred){
    FILE *pipe;
    char buff[512];
    std::string env;
    env += "COMMIT=" + commit;
    env += " REF_NAME='" + ref_name + "'";
    env += " DESCRIPTION='" + description + "'";
    env += " PULL_URI='" + pull_uri + "'";
    if (cred.access_token.size()){
        env += " AUTHPLUS_ACCESS_TOKEN='" + cred.access_token + "'";
    }
    if (cred.ca_file.size()){
        env += " TLS_CA_CERT='" + cred.ca_file + "'";
    }
    if (cred.cert_file.size()){
        env += " TLS_CLIENT_CERT='" + cred.cert_file + "'";
    }
    if (cred.pkey_file.size()){
        env += " TLS_CLIENT_KEY='" + cred.pkey_file + "' ";
    }

    std::string output;
    std::string ostree_script = "sota_ostree.sh";
    if(!(pipe = popen((env + ostree_script).c_str(), "r"))){
        throw std::runtime_error("could not find or execute: " + ostree_script);
    }
    while(fgets(buff, sizeof(buff), pipe) != NULL){
        output += buff;
    }
    int status_code = pclose(pipe)/256;
    if (status_code == 0){
       // FIXME write json to NEW_PACKAGE
        return InstallOutcome(data::OK, output);
    }else if (status_code == 99){
        return InstallOutcome(data::ALREADY_PROCESSED, output);
    }
    return InstallOutcome(data::INSTALL_FAILED, output);
}

bool
ot_admin_builtin_status ()
{
  OstreeSysroot *sysroot = NULL;
  bool ret = FALSE;
  OstreeRepo *repo = NULL;
  //OstreeDeployment *booted_deployment = NULL;
  g_autoptr(GPtrArray) deployments = NULL;
  uint i;

sysroot = ostree_sysroot_new_default();
GCancellable *cancellable = NULL;
GError **error = NULL;
  if (!ostree_sysroot_load (sysroot, cancellable, error))
    goto out;

  if (!ostree_sysroot_get_repo (sysroot, &repo, cancellable, error))
    goto out;

  deployments = ostree_sysroot_get_deployments (sysroot);
  //booted_deployment = ostree_sysroot_get_booted_deployment (sysroot);

  if (deployments->len == 0)
    {
      g_print ("No deployments.\n");
    }
  else
    {
      for (i = 0; i < deployments->len; i++)
        {
            OstreeDeployment *deployment = (OstreeDeployment*)deployments->pdata[i];
          //GKeyFile *origin;
          const char *ref = ostree_deployment_get_csum (deployment);
          std::cout << ref << "\n\n\n\n";
        }
    }

  ret = TRUE;
 out:
  return ret;
}


