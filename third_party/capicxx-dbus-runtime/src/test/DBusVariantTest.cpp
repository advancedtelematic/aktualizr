// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <gtest/gtest.h>

#ifndef COMMONAPI_INTERNAL_COMPILATION
#define COMMONAPI_INTERNAL_COMPILATION
#endif

#include <CommonAPI/Variant.hpp>

using namespace CommonAPI;

class VariantTest: public ::testing::Test {

  protected:
    typedef Variant<int, double, std::string> BasicVariantType;

    void SetUp() {
        fromInt = 5;
        fromDouble = 12.344;
        fromString = "123abcsadfaljkawlöfasklöerklöfjasklfjysklfjaskfjsklösdfdko4jdfasdjioögjopefgip3rtgjiprg!";
    }

    void TearDown() {
    }

    int fromInt;
    double fromDouble;
    std::string fromString;
};

TEST_F(VariantTest, HandlesInts) {
    BasicVariantType myVariant(fromInt);

    EXPECT_TRUE(myVariant.isType<int>());
    EXPECT_FALSE(myVariant.isType<double>());
    EXPECT_FALSE(myVariant.isType<std::string>());

    const int myInt = myVariant.get<int>();
    EXPECT_EQ(myInt, fromInt);

    EXPECT_ANY_THROW(myVariant.get<double>());
    EXPECT_ANY_THROW(myVariant.get<std::string>());
}

TEST_F(VariantTest, HandlesDoubles) {
    BasicVariantType myVariant(fromDouble);

    EXPECT_FALSE(myVariant.isType<int>());
    EXPECT_TRUE(myVariant.isType<double>());
    EXPECT_FALSE(myVariant.isType<std::string>());

    EXPECT_ANY_THROW(myVariant.get<int>());

    const double myDouble = myVariant.get<double>();
    EXPECT_EQ(myDouble, fromDouble);

    EXPECT_ANY_THROW(myVariant.get<std::string>());
}

TEST_F(VariantTest, HandlesStrings) {
    BasicVariantType myVariant(fromString);

    EXPECT_FALSE(myVariant.isType<int>());
    EXPECT_FALSE(myVariant.isType<double>());
    EXPECT_TRUE(myVariant.isType<std::string>());

    EXPECT_ANY_THROW(myVariant.get<int>());
    EXPECT_ANY_THROW(myVariant.get<double>());

    const std::string myString = myVariant.get<std::string>();
    EXPECT_EQ(myString, fromString);
}

TEST_F(VariantTest, HandlesStringVectors) {
    typedef Variant<int, double, std::vector<std::string>> VectorVariantType;

    std::vector<std::string> testVector;
    for(int i = 0; i < 10; i++) {
        testVector.push_back(fromString);
    }

    VectorVariantType vectorVariant(testVector);
    const std::vector<std::string> resultVector = vectorVariant.get<std::vector<std::string>>();
    EXPECT_EQ(resultVector, testVector);
}

TEST_F(VariantTest, HandlesAssignment) {
    Variant<int, double, std::string> myVariant = fromInt;

    myVariant = fromString;

    EXPECT_FALSE(myVariant.isType<int>());
    EXPECT_FALSE(myVariant.isType<double>());
    EXPECT_TRUE(myVariant.isType<std::string>());

    EXPECT_ANY_THROW(myVariant.get<int>());

    const std::string myString = myVariant.get<std::string>();
    EXPECT_EQ(myString, fromString);
}

TEST_F(VariantTest, HandlesVariantConstructionByAssignment) {
    Variant<int, double, std::string> myVariant = fromInt;

    EXPECT_TRUE(myVariant.isType<int>());
    EXPECT_FALSE(myVariant.isType<double>());
    EXPECT_FALSE(myVariant.isType<std::string>());

    const int myInt = myVariant.get<int>();
    EXPECT_EQ(myInt, fromInt);
}

TEST_F(VariantTest, HandlesVariantCopy) {
    BasicVariantType myVariant(fromInt);
    Variant<int, double, std::string> myVariantAssigned = myVariant;

    EXPECT_TRUE(myVariantAssigned.isType<int>());
    EXPECT_FALSE(myVariantAssigned.isType<double>());
    EXPECT_FALSE(myVariantAssigned.isType<std::string>());

    const int myInt2 = myVariantAssigned.get<int>();
    EXPECT_EQ(myInt2, fromInt);

    Variant<int, double, std::string> myVariantCopied(myVariant);
    EXPECT_EQ(myVariant, myVariantCopied);

    EXPECT_TRUE(myVariantCopied.isType<int>());
    EXPECT_FALSE(myVariantCopied.isType<double>());
    EXPECT_FALSE(myVariantCopied.isType<std::string>());

    const int& myIntCopy = myVariantCopied.get<int>();
    EXPECT_EQ(myIntCopy, fromInt);
}

TEST_F(VariantTest, HandlesVariantsWithinVariants) {
    typedef Variant<int, double, BasicVariantType > VariantInVariantType;
    BasicVariantType fromInnerVariant(fromInt);
    VariantInVariantType myOuterVariant(fromInnerVariant);

    EXPECT_FALSE(myOuterVariant.isType<int>());
    EXPECT_FALSE(myOuterVariant.isType<double>());
    EXPECT_TRUE(myOuterVariant.isType<BasicVariantType>());

    EXPECT_ANY_THROW(myOuterVariant.get<int>());
    EXPECT_ANY_THROW(myOuterVariant.get<double>());

    const BasicVariantType myInnerVariant = myOuterVariant.get<BasicVariantType>();
    EXPECT_EQ(fromInnerVariant, myInnerVariant);
}


TEST_F(VariantTest, VariantStringArray) {

    std::vector<std::string> testVector;
    testVector.push_back("Test 1");
    testVector.push_back("Test 2");
    testVector.push_back("Test 3");
    testVector.push_back("Test 4");

    CommonAPI::Variant<int, double, std::vector<std::string>>* vectorVariant = new CommonAPI::Variant<int, double, std::vector<std::string>>(testVector);

    std::vector<std::string> readVector = vectorVariant->get<std::vector<std::string> >();
    EXPECT_EQ(readVector, testVector);

    delete vectorVariant;
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
#endif
