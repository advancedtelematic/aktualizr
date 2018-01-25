#include <gtest/gtest.h>

#include <string>

#include <boost/smart_ptr.hpp>

#include "uptane/secondaryconfig.h"

#include "asn1/helpers.h"

TEST(asn1, serialize_simple) {
  AKSecondaryConfig *secConf = static_cast<AKSecondaryConfig *>(calloc(1, sizeof(*secConf)));

  secConf->secondaryType = AKSecondaryType_virtual;
  secConf->partialVerifying = 0;
  secConf->ecuSerial = OCTET_STRING_new_fromBuf(&asn_DEF_OCTET_STRING, "serial", -1);
  OCTET_STRING_fromString(&secConf->ecuHardwareId, "hwid");

  OCTET_STRING_fromString(&secConf->fullClientDir, "/d1");
  OCTET_STRING_fromString(&secConf->ecuPrivateKey, "priv.key");
  OCTET_STRING_fromString(&secConf->ecuPublicKey, "pub.key");
  OCTET_STRING_fromString(&secConf->firmwarePath, "/firm.bin");
  OCTET_STRING_fromString(&secConf->targetNamePath, "/target");
  OCTET_STRING_fromString(&secConf->metadataPath, "/meta");

  EXPECT_EQ(ASN1::xer_serialize(static_cast<void *>(secConf), &asn_DEF_AKSecondaryConfig),
            "<AKSecondaryConfig><secondaryType><virtual/></secondaryType><partialVerifying><false/></"
            "partialVerifying><ecuSerial>serial</ecuSerial><ecuHardwareId>hwid</ecuHardwareId><fullClientDir>/d1</"
            "fullClientDir><ecuPrivateKey>priv.key</ecuPrivateKey><ecuPublicKey>pub.key</ecuPublicKey><firmwarePath>/"
            "firm.bin</firmwarePath><targetNamePath>/target</targetNamePath><metadataPath>/meta</metadataPath></"
            "AKSecondaryConfig>");

  ASN_STRUCT_FREE(asn_DEF_AKSecondaryConfig, secConf);
}

TEST(asn1, deserialize_simple) {
  ASSERT_THROW(ASN1::xer_parse<AKSecondaryConfig>(&asn_DEF_AKSecondaryConfig, "hey"), ASN1::DecodeError);

  std::string withSerial =
      "<AKSecondaryConfig><secondaryType><virtual/></secondaryType><partialVerifying><false/></"
      "partialVerifying><ecuSerial>serial</ecuSerial><ecuHardwareId>hwid</ecuHardwareId><fullClientDir>/d1</"
      "fullClientDir><ecuPrivateKey>priv.key</ecuPrivateKey><ecuPublicKey>pub.key</ecuPublicKey><firmwarePath>/"
      "firm.bin</firmwarePath><targetNamePath>/target</targetNamePath><metadataPath>/meta</metadataPath></"
      "AKSecondaryConfig>";
  ASN1::xer_parse<AKSecondaryConfig>(&asn_DEF_AKSecondaryConfig, withSerial);

  std::string withoutSerial =
      "<AKSecondaryConfig><secondaryType><virtual/></secondaryType><partialVerifying><false/></"
      "partialVerifying><ecuHardwareId>hwid</ecuHardwareId><fullClientDir>/d1</"
      "fullClientDir><ecuPrivateKey>priv.key</ecuPrivateKey><ecuPublicKey>pub.key</ecuPublicKey><firmwarePath>/"
      "firm.bin</firmwarePath><targetNamePath>/target</targetNamePath><metadataPath>/meta</metadataPath></"
      "AKSecondaryConfig>";
  ASN1::xer_parse<AKSecondaryConfig>(&asn_DEF_AKSecondaryConfig, withoutSerial);
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
