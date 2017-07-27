#ifndef UPTANE_SECONDARYCONFIG_H_
#define UPTANE_SECONDARYCONFIG_H_
namespace Uptane{
struct SecondaryConfig{
    std::string ecu_serial;
    std::string ecu_hardware_id;
    std::string ecu_private_key;
    std::string ecu_public_key;
    boost::filesystem::path full_client_dir;
    bool partial_verifying;
    boost::filesystem::path firmware_path;
};
}

#endif
