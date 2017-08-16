/* Json snippet returned by sendMetaXXX(): 
 * {
 *   valid = true/false,
 *   wait_for_target = true/false
 * }
 */

namespace Uptane {
  class ISecondaryBus {
    public:
	    virtual ~ISecondaryBus() {;}
	    virtual Json::Value getManifest(std::string& ecu_serial) = 0;
	    virtual Json::Value sendMetaPartial(const TimeMeta& time_meta, const Root& root_meta, const Targets& targets_meta) = 0;
	    virtual Json::Value sendMetaFull(const TimeMeta& time_meta, const MetaPack& meta_pack) = 0;
	    virtual bool sendFirmware(const uint8_t* blob, size_t size) = 0;
	    virtual void getPublicKey(const std::string &ecu_serial, std::string *keytype, std::string * key) = 0;
	    // For special cases like virtual ECUs and legacy non-uptane ECUs keys are set by the primary
	    virtual void setKeys(const std::string &ecu_serial, const std::string &keytype, const std::string &public_key, const std::string &private_key) = 0;
  }
}
