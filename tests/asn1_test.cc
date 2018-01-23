#include <gtest/gtest.h>

#include <string>

#include <boost/smart_ptr.hpp>

#include <SecondaryConfig.h>
#include "asn1/helpers.h"

using namespace Uptane::ASN1;

TEST(asn1, serialize_simple) {
  SecondaryConfig *secConf = static_cast<SecondaryConfig *>(calloc(1, sizeof(*secConf)));

  secConf->secondaryType = SecondaryType_virtual;
  secConf->partialVerifying = 0;
  secConf->ecuSerial = OCTET_STRING_new_fromBuf(&asn_DEF_OCTET_STRING, "serial", -1);
  OCTET_STRING_fromString(&secConf->ecuHardwareId, "hwid");

  OCTET_STRING_fromString(&secConf->fullClientDir, "/d1");
  OCTET_STRING_fromString(&secConf->ecuPrivateKey, "priv.key");
  OCTET_STRING_fromString(&secConf->ecuPublicKey, "pub.key");
  OCTET_STRING_fromString(&secConf->firmwarePath, "/firm.bin");
  OCTET_STRING_fromString(&secConf->targetNamePath, "/target");
  OCTET_STRING_fromString(&secConf->metadataPath, "/meta");

  EXPECT_EQ(xer_serialize(static_cast<void *>(secConf), &asn_DEF_SecondaryConfig),
            "<SecondaryConfig><secondaryType><virtual/></secondaryType><partialVerifying><false/></"
            "partialVerifying><ecuSerial>serial</ecuSerial><ecuHardwareId>hwid</ecuHardwareId><fullClientDir>/d1</"
            "fullClientDir><ecuPrivateKey>priv.key</ecuPrivateKey><ecuPublicKey>pub.key</ecuPublicKey><firmwarePath>/"
            "firm.bin</firmwarePath><targetNamePath>/target</targetNamePath><metadataPath>/meta</metadataPath></"
            "SecondaryConfig>");

  ASN_STRUCT_FREE(asn_DEF_SecondaryConfig, secConf);
}

TEST(asn1, deserialize_simple) {
  ASSERT_THROW(xer_parse<SecondaryConfig>(asn_DEF_SecondaryConfig, "hey"), DecodeError);

  std::string withSerial =
      "<SecondaryConfig><secondaryType><virtual/></secondaryType><partialVerifying><false/></"
      "partialVerifying><ecuSerial>serial</ecuSerial><ecuHardwareId>hwid</ecuHardwareId><fullClientDir>/d1</"
      "fullClientDir><ecuPrivateKey>priv.key</ecuPrivateKey><ecuPublicKey>pub.key</ecuPublicKey><firmwarePath>/"
      "firm.bin</firmwarePath><targetNamePath>/target</targetNamePath><metadataPath>/meta</metadataPath></"
      "SecondaryConfig>";
  xer_parse<SecondaryConfig>(asn_DEF_SecondaryConfig, withSerial);

  std::string withoutSerial =
      "<SecondaryConfig><secondaryType><virtual/></secondaryType><partialVerifying><false/></"
      "partialVerifying><ecuHardwareId>hwid</ecuHardwareId><fullClientDir>/d1</"
      "fullClientDir><ecuPrivateKey>priv.key</ecuPrivateKey><ecuPublicKey>pub.key</ecuPublicKey><firmwarePath>/"
      "firm.bin</firmwarePath><targetNamePath>/target</targetNamePath><metadataPath>/meta</metadataPath></"
      "SecondaryConfig>";
  xer_parse<SecondaryConfig>(asn_DEF_SecondaryConfig, withoutSerial);
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
