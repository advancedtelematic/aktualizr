// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <gtest/gtest.h>

#ifndef COMMONAPI_INTERNAL_COMPILATION
#define COMMONAPI_INTERNAL_COMPILATION
#endif

#include <CommonAPI/DBus/DBusAddress.hpp>
#include <CommonAPI/DBus/DBusOutputStream.hpp>
#include <CommonAPI/DBus/DBusInputStream.hpp>

using namespace CommonAPI;

class VariantOutputStreamTest: public ::testing::Test {
  protected:
    size_t numOfElements;
    CommonAPI::DBus::DBusMessage message;
    const char* busName;
    const char* objectPath;
    const char* interfaceName;
    const char* methodName;
    const char* signature;

    void SetUp() {
      numOfElements = 10;
      busName = "no.bus.here";
      objectPath = "/no/object/here";
      interfaceName = "no.interface.here";
      methodName = "noMethodHere";
      signature = "yyyyyyyyyy";
    }

    void TearDown() {
    }

};

typedef Variant<int,bool> InnerVar;

struct MyStruct: CommonAPI::Struct<uint32_t, InnerVar, bool, std::string, double> {
    ~MyStruct();

    virtual uint32_t    getA()                { return std::get<0>(values_); }
    virtual InnerVar    getB()                { return std::get<1>(values_); }
    virtual bool        getC()                { return std::get<2>(values_); }
    virtual std::string getD()                { return std::get<3>(values_); }
    virtual double      getE()                { return std::get<4>(values_); }

    virtual void        setA(uint32_t a)      { std::get<0>(values_) = a; }
    virtual void        setB(InnerVar b)      { std::get<1>(values_) = b; }
    virtual void        setC(bool c)          { std::get<2>(values_) = c; }
    virtual void        setD(std::string d)   { std::get<3>(values_) = d; }
    virtual void        setE(double e)        { std::get<4>(values_) = e; }
};

MyStruct::~MyStruct() {
}

bool operator==(const MyStruct& lhs, const MyStruct& rhs) {
    if (&lhs == &rhs)
        return true;

    return (std::get<0>(lhs.values_) == std::get<0>(rhs.values_))
        && (std::get<1>(lhs.values_) == std::get<1>(rhs.values_))
        && (std::get<2>(lhs.values_) == std::get<2>(rhs.values_))
        && (std::get<3>(lhs.values_) == std::get<3>(rhs.values_))
        && (std::get<4>(lhs.values_) == std::get<4>(rhs.values_));
    ;
}

TEST_F(VariantOutputStreamTest, CanBeCalled) {
    message = CommonAPI::DBus::DBusMessage::createMethodCall(CommonAPI::DBus::DBusAddress(busName, objectPath, interfaceName), methodName, signature);
    DBus::DBusOutputStream outputStream(message);
}

TEST_F(VariantOutputStreamTest, CanWriteVariant) {
    message = CommonAPI::DBus::DBusMessage::createMethodCall(CommonAPI::DBus::DBusAddress(busName, objectPath, interfaceName), methodName, signature);
    DBus::DBusOutputStream outputStream(message);

    int fromInt = 14132;
    Variant<int, bool> inVariant(fromInt);
    Variant<int, bool> outVariant;
    outputStream.writeValue(inVariant, static_cast<CommonAPI::DBus::VariantDeployment<CommonAPI::EmptyDeployment, CommonAPI::EmptyDeployment>*>(nullptr));
    outputStream.flush();

    DBus::DBusInputStream inputStream(message);

    inputStream.readValue(outVariant, static_cast<CommonAPI::DBus::VariantDeployment<CommonAPI::EmptyDeployment, CommonAPI::EmptyDeployment>*>(nullptr));

    EXPECT_TRUE(outVariant.isType<int>());
    EXPECT_EQ(inVariant.get<int>(), outVariant.get<int>());
    EXPECT_TRUE(inVariant == outVariant);
}

TEST_F(VariantOutputStreamTest, CanWriteVariantInVariant) {
    message = CommonAPI::DBus::DBusMessage::createMethodCall(CommonAPI::DBus::DBusAddress(busName, objectPath, interfaceName), methodName, signature);
    DBus::DBusOutputStream outputStream(message);

    int fromInt = 14132;
    Variant<int, bool> nestedVariant(fromInt);

    Variant<InnerVar, std::string, float> inVariant(nestedVariant);

    Variant<InnerVar, std::string, float> outVariant;

    outputStream.writeValue(inVariant, static_cast<CommonAPI::DBus::VariantDeployment<CommonAPI::DBus::VariantDeployment<EmptyDeployment, EmptyDeployment>, EmptyDeployment, EmptyDeployment>*>(nullptr));
    outputStream.flush();

    DBus::DBusInputStream inputStream(message);
    
    inputStream.readValue(outVariant, static_cast<CommonAPI::DBus::VariantDeployment<CommonAPI::DBus::VariantDeployment<EmptyDeployment, EmptyDeployment>, EmptyDeployment, EmptyDeployment>*>(nullptr));

    EXPECT_TRUE(outVariant.isType<InnerVar>());
    EXPECT_EQ(inVariant.get<InnerVar>(), outVariant.get<InnerVar>());
    EXPECT_TRUE(inVariant == outVariant);
}

TEST_F(VariantOutputStreamTest, CanWriteVariantInStruct) {
    message = CommonAPI::DBus::DBusMessage::createMethodCall(CommonAPI::DBus::DBusAddress(busName, objectPath, interfaceName), methodName, signature);
    DBus::DBusOutputStream outputStream(message);

    int fromInt = 14132;
    Variant<int, bool> nestedVariant(fromInt);

    MyStruct inStruct;
    inStruct.setB(nestedVariant);

    MyStruct outStruct;

    outputStream << inStruct;
    outputStream.flush();

    DBus::DBusInputStream inputStream(message);

    inputStream >> outStruct;

    EXPECT_TRUE(outStruct.getB().isType<int>());
    EXPECT_EQ(outStruct.getB().get<int>(), inStruct.getB().get<int>());
    EXPECT_TRUE(inStruct.getB() == outStruct.getB());
}

TEST_F(VariantOutputStreamTest, CanWriteVariantInArray) {
    message = CommonAPI::DBus::DBusMessage::createMethodCall(CommonAPI::DBus::DBusAddress(busName, objectPath, interfaceName), methodName, signature);
    DBus::DBusOutputStream outputStream(message);

    int fromInt = 14132;
    std::vector<InnerVar> inVector;
    std::vector<InnerVar> outVector;

    for (unsigned int i = 0; i < numOfElements; i++) {
        inVector.push_back(InnerVar(fromInt));
    }

    outputStream << inVector;
    outputStream.flush();

    DBus::DBusInputStream inputStream(message);

    inputStream >> outVector;

    EXPECT_TRUE(outVector[0].isType<int>());
    EXPECT_EQ(inVector[0].get<int>(), outVector[0].get<int>());
    EXPECT_EQ(numOfElements, outVector.size());
    EXPECT_TRUE(inVector[0] == outVector[0]);
}

TEST_F(VariantOutputStreamTest, CanWriteArrayInVariant) {
    message = CommonAPI::DBus::DBusMessage::createMethodCall(CommonAPI::DBus::DBusAddress(busName, objectPath, interfaceName), methodName, signature);
    DBus::DBusOutputStream outputStream(message);

    typedef std::vector<int> IntVector;
    typedef Variant<IntVector, std::string> VectorVariant;

    std::vector<int> inVector;
    int fromInt = 14132;
    for (unsigned int i = 0; i < numOfElements; i++) {
        inVector.push_back(fromInt);
    }


    VectorVariant inVariant(inVector);
    VectorVariant outVariant;

    outputStream << inVariant;
    outputStream.flush();

    DBus::DBusInputStream inputStream(message);

    inputStream >> outVariant;

    EXPECT_TRUE(outVariant.isType<IntVector>());
    EXPECT_EQ(inVariant.get<IntVector>(), outVariant.get<IntVector>());
    EXPECT_TRUE(inVariant == outVariant);
}

TEST_F(VariantOutputStreamTest, CanWriteStructInVariant) {
    message = CommonAPI::DBus::DBusMessage::createMethodCall(CommonAPI::DBus::DBusAddress(busName, objectPath, interfaceName), methodName, signature);
    DBus::DBusOutputStream outputStream(message);

    typedef Variant<MyStruct, std::string> StructVariant;

    MyStruct str;
    int fromInt = 14132;
    str.setA(fromInt);

    StructVariant inVariant(str);
    StructVariant outVariant;

    outputStream << inVariant;
    outputStream.flush();

    DBus::DBusInputStream inputStream(message);

    inputStream >> outVariant;

    EXPECT_TRUE(outVariant.isType<MyStruct>());
    MyStruct iStr = inVariant.get<MyStruct>();
    MyStruct oStr = outVariant.get<MyStruct>();
    EXPECT_EQ(iStr, oStr);
    EXPECT_TRUE(inVariant == outVariant);
}

TEST_F(VariantOutputStreamTest, CanWriteVariantInStructInVariant) {
    message = CommonAPI::DBus::DBusMessage::createMethodCall(CommonAPI::DBus::DBusAddress(busName, objectPath, interfaceName), methodName, signature);
    DBus::DBusOutputStream outputStream(message);

    typedef Variant<MyStruct, std::string> StructVariant;

    MyStruct str;
    int fromInt = 14132;
    str.setB(InnerVar(fromInt));

    StructVariant inVariant(str);
    StructVariant outVariant;

    outputStream << inVariant;
    outputStream.flush();

    DBus::DBusInputStream inputStream(message);

    inputStream >> outVariant;

    EXPECT_TRUE(outVariant.isType<MyStruct>());
    EXPECT_EQ(inVariant.get<MyStruct>(), outVariant.get<MyStruct>());
    EXPECT_TRUE(std::get<1>(inVariant.get<MyStruct>().values_) == std::get<1>(outVariant.get<MyStruct>().values_));
    EXPECT_TRUE(std::get<1>(inVariant.get<MyStruct>().values_).get<int>() == std::get<1>(outVariant.get<MyStruct>().values_).get<int>());
    EXPECT_TRUE(inVariant == outVariant);
}

TEST_F(VariantOutputStreamTest, CanWriteVariantInArrayInVariant) {
    message = CommonAPI::DBus::DBusMessage::createMethodCall(CommonAPI::DBus::DBusAddress(busName, objectPath, interfaceName), methodName, signature);
    DBus::DBusOutputStream outputStream(message);

    typedef std::vector<InnerVar> VarVector;
    typedef Variant<VarVector, std::string> ArrayVariant;

    VarVector inVector;
    int fromInt = 14132;
    for (unsigned int i = 0; i < numOfElements; i++) {
        inVector.push_back(InnerVar(fromInt));
    }

    ArrayVariant inVariant(inVector);
    ArrayVariant outVariant;

    outputStream << inVariant;
    outputStream.flush();

    DBus::DBusInputStream inputStream(message);

    inputStream >> outVariant;

    EXPECT_TRUE(outVariant.isType<VarVector>());
    EXPECT_EQ(inVariant.get<VarVector>(), outVariant.get<VarVector>());
    EXPECT_TRUE(inVariant.get<VarVector>()[0] == outVariant.get<VarVector>()[0]);
    EXPECT_TRUE(inVariant.get<VarVector>()[0].get<int>() == outVariant.get<VarVector>()[0].get<int>());
    EXPECT_TRUE(inVariant == outVariant);
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
#endif
