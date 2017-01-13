// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_DBUS_DBUSINPUTSTREAM_HPP_
#define COMMONAPI_DBUS_DBUSINPUTSTREAM_HPP_

#include <iostream>
#include <iomanip>
#include <sstream>

#include <cassert>
#include <cstdint>
#include <stack>
#include <string>
#include <vector>

#include <CommonAPI/Export.hpp>
#include <CommonAPI/InputStream.hpp>
#include <CommonAPI/Struct.hpp>
#include <CommonAPI/DBus/DBusDeployment.hpp>
#include <CommonAPI/DBus/DBusError.hpp>
#include <CommonAPI/DBus/DBusFreedesktopVariant.hpp>
#include <CommonAPI/DBus/DBusHelper.hpp>
#include <CommonAPI/DBus/DBusMessage.hpp>

namespace CommonAPI {
namespace DBus {

// Used to mark the position of a pointer within an array of bytes.
typedef uint32_t position_t;

/**
 * @class DBusInputMessageStream
 *
 * Used to deserialize and read data from a #DBusMessage. For all data types that can be read from a #DBusMessage, a ">>"-operator should be defined to handle the reading
 * (this operator is predefined for all basic data types and for vectors).
 */
class DBusInputStream
    : public InputStream<DBusInputStream> {
public:
    COMMONAPI_EXPORT bool hasError() const {
        return isErrorSet();
    }

    COMMONAPI_EXPORT InputStream &readValue(bool &_value, const EmptyDeployment *_depl);

    COMMONAPI_EXPORT InputStream &readValue(int8_t &_value, const EmptyDeployment *_depl);
    COMMONAPI_EXPORT InputStream &readValue(int16_t &_value, const EmptyDeployment *_depl);
    COMMONAPI_EXPORT InputStream &readValue(int32_t &_value, const EmptyDeployment *_depl);
    COMMONAPI_EXPORT InputStream &readValue(int64_t &_value, const EmptyDeployment *_depl);

    COMMONAPI_EXPORT InputStream &readValue(uint8_t &_value, const EmptyDeployment *_depl);
    COMMONAPI_EXPORT InputStream &readValue(uint16_t &_value, const EmptyDeployment *_depl);
    COMMONAPI_EXPORT InputStream &readValue(uint32_t &_value, const EmptyDeployment *_depl);
    COMMONAPI_EXPORT InputStream &readValue(uint64_t &_value, const EmptyDeployment *_depl);

    COMMONAPI_EXPORT InputStream &readValue(float &_value, const EmptyDeployment *_depl);
    COMMONAPI_EXPORT InputStream &readValue(double &_value, const EmptyDeployment *_depl);

    COMMONAPI_EXPORT InputStream &readValue(std::string &_value, const EmptyDeployment *_depl);

    COMMONAPI_EXPORT InputStream &readValue(std::string &_value, const CommonAPI::DBus::StringDeployment* _depl) {
        (void)_depl;
        return readValue(_value, static_cast<EmptyDeployment *>(nullptr));
    }

    COMMONAPI_EXPORT InputStream &readValue(Version &_value, const EmptyDeployment *_depl);

    COMMONAPI_EXPORT void beginReadMapOfSerializableStructs() {
        uint32_t itsSize;
        _readValue(itsSize);
        pushSize(itsSize);
        align(8); /* correct alignment for first DICT_ENTRY */
        pushPosition();
    }

    COMMONAPI_EXPORT bool readMapCompleted() {
        return (sizes_.top() <= (current_ - positions_.top()));
    }

    COMMONAPI_EXPORT void endReadMapOfSerializableStructs() {
        (void)popSize();
        (void)popPosition();
    }

    COMMONAPI_EXPORT InputStream &skipMap() {
        uint32_t itsSize;
        _readValue(itsSize);
        align(8); /* skip padding (if any) */
        assert(itsSize <= (sizes_.top() + positions_.top() - current_));
        _readRaw(itsSize);
        return (*this);
    }

    template<class Deployment_, typename Base_>
    COMMONAPI_EXPORT InputStream &readValue(Enumeration<Base_> &_value, const Deployment_ *_depl) {
        Base_ tmpValue;
        readValue(tmpValue, _depl);
        _value = tmpValue;
        return (*this);
    }

    template<class Deployment_, typename... Types_>
    COMMONAPI_EXPORT InputStream &readValue(Struct<Types_...> &_value, const Deployment_ *_depl) {
        align(8);
        const auto itsSize(std::tuple_size<std::tuple<Types_...>>::value);
        StructReader<itsSize-1, DBusInputStream, Struct<Types_...>, Deployment_>{}(
            (*this), _value, _depl);
        return (*this);
    }

    template<class Deployment_, class PolymorphicStruct_>
    COMMONAPI_EXPORT InputStream &readValue(std::shared_ptr<PolymorphicStruct_> &_value,
                           const Deployment_ *_depl) {
        uint32_t serial;
        align(8);
        _readValue(serial);
        skipSignature();
        align(8);
        if (!hasError()) {
            _value = PolymorphicStruct_::create(serial);
            _value->template readValue<>(*this, _depl);
        }

        return (*this);
    }

    template<typename... Types_>
    COMMONAPI_EXPORT InputStream &readValue(Variant<Types_...> &_value, const CommonAPI::EmptyDeployment *_depl = nullptr) {
        (void)_depl;
        if(_value.hasValue()) {
            DeleteVisitor<_value.maxSize> visitor(_value.valueStorage_);
            ApplyVoidVisitor<DeleteVisitor<_value.maxSize>,
                Variant<Types_...>, Types_... >::visit(visitor, _value);
        }

        align(8);
        readValue(_value.valueType_, static_cast<EmptyDeployment *>(nullptr));
        skipSignature();

        InputStreamReadVisitor<DBusInputStream, Types_...> visitor((*this), _value);
        ApplyVoidVisitor<InputStreamReadVisitor<DBusInputStream, Types_... >,
            Variant<Types_...>, Types_...>::visit(visitor, _value);

        return (*this);
    }

    template<typename Deployment_, typename... Types_>
    COMMONAPI_EXPORT InputStream &readValue(Variant<Types_...> &_value, const Deployment_ *_depl) {
        if(_value.hasValue()) {
            DeleteVisitor<_value.maxSize> visitor(_value.valueStorage_);
            ApplyVoidVisitor<DeleteVisitor<_value.maxSize>,
                Variant<Types_...>, Types_... >::visit(visitor, _value);
        }

        if (_depl != nullptr && _depl->isDBus_) {
            // Read signature
            uint8_t signatureLength;
            readValue(signatureLength, static_cast<EmptyDeployment *>(nullptr));
            std::string signature(_readRaw(signatureLength+1), signatureLength);

            // Determine index (value type) from signature
            TypeCompareVisitor<Types_...> visitor(signature);
            bool success = ApplyTypeCompareVisitor<
                                    TypeCompareVisitor<Types_...>,
                                    Variant<Types_...>,
                                    Deployment_,
                                    Types_...
                                >::visit(visitor, _value, _depl, _value.valueType_);
            if (!success) {
                _value.valueType_ = 0; // Invalid index
                setError();
                return (*this);
            }
        } else {
            align(8);
            readValue(_value.valueType_, static_cast<EmptyDeployment *>(nullptr));
            skipSignature();
        }


        InputStreamReadVisitor<DBusInputStream, Types_...> visitor((*this), _value);
        ApplyStreamVisitor<InputStreamReadVisitor<DBusInputStream, Types_... >,
            Variant<Types_...>, Deployment_, Types_...>::visit(visitor, _value, _depl);

        return (*this);
    }

    template<typename ElementType_>
    COMMONAPI_EXPORT InputStream &readValue(std::vector<ElementType_> &_value, const EmptyDeployment *_depl) {
        (void)_depl;

        uint32_t itsSize;
        _readValue(itsSize);
        pushSize(itsSize);

        alignVector<ElementType_>();

        pushPosition();

        _value.clear();
        while (sizes_.top() > current_ - positions_.top()) {
            ElementType_ itsElement;
            readValue(itsElement, static_cast<EmptyDeployment *>(nullptr));

            if (hasError()) {
                break;
            }

            _value.push_back(std::move(itsElement));
        }

        popSize();
        popPosition();

        return (*this);
    }

    template<class Deployment_, typename ElementType_>
    COMMONAPI_EXPORT InputStream &readValue(std::vector<ElementType_> &_value, const Deployment_ *_depl) {
        uint32_t itsSize;
        _readValue(itsSize);
        pushSize(itsSize);

        alignVector<ElementType_>();

        pushPosition();

        _value.clear();
        while (sizes_.top() > current_ - positions_.top()) {
            ElementType_ itsElement;
            readValue(itsElement, (_depl ? _depl->elementDepl_ : nullptr));

            if (hasError()) {
                break;
            }

            _value.push_back(std::move(itsElement));
        }

        popSize();
        popPosition();

        return (*this);
    }

    template<typename KeyType_, typename ValueType_, typename HasherType_>
    COMMONAPI_EXPORT InputStream &readValue(std::unordered_map<KeyType_, ValueType_, HasherType_> &_value,
                           const EmptyDeployment *_depl) {

        typedef typename std::unordered_map<KeyType_, ValueType_, HasherType_>::value_type MapElement;

        uint32_t itsSize;
        _readValue(itsSize);
        pushSize(itsSize);

        align(8);
        pushPosition();

        _value.clear();
        while (sizes_.top() > current_ - positions_.top()) {
            KeyType_ itsKey;
            ValueType_ itsValue;

            align(8);
            readValue(itsKey, _depl);
            readValue(itsValue, _depl);

            if (hasError()) {
                break;
            }

            _value.insert(MapElement(std::move(itsKey), std::move(itsValue)));
        }

        (void)popSize();
        (void)popPosition();

        return (*this);
    }

    template<class Deployment_, typename KeyType_, typename ValueType_, typename HasherType_>
    COMMONAPI_EXPORT InputStream &readValue(std::unordered_map<KeyType_, ValueType_, HasherType_> &_value,
                           const Deployment_ *_depl) {

        typedef typename std::unordered_map<KeyType_, ValueType_, HasherType_>::value_type MapElement;

        uint32_t itsSize;
        _readValue(itsSize);
        pushSize(itsSize);

        align(8);
        pushPosition();

        _value.clear();
        while (sizes_.top() > current_ - positions_.top()) {
            KeyType_ itsKey;
            ValueType_ itsValue;

            align(8);
            readValue(itsKey, (_depl ? _depl->key_ : nullptr));
            readValue(itsValue, (_depl ? _depl->value_ : nullptr));

            if (hasError()) {
                break;
            }

            _value.insert(MapElement(std::move(itsKey), std::move(itsValue)));
        }

        (void)popSize();
        (void)popPosition();

        return (*this);
    }

    /**
     * Creates a #DBusInputMessageStream which can be used to deserialize and read data from the given #DBusMessage.
     * As no message-signature is checked, the user is responsible to ensure that the correct data types are read in the correct order.
     *
     * @param message the #DBusMessage from which data should be read.
     */
    COMMONAPI_EXPORT DBusInputStream(const CommonAPI::DBus::DBusMessage &_message);
    COMMONAPI_EXPORT DBusInputStream(const DBusInputStream &_stream) = delete;

    /**
     * Destructor; does not call the destructor of the referred #DBusMessage. Make sure to maintain a reference to the
     * #DBusMessage outside of the stream if you intend to make further use of the message.
     */
    COMMONAPI_EXPORT ~DBusInputStream();

    // Marks the stream as erroneous.
    COMMONAPI_EXPORT void setError();

    /**
     * @return An instance of #DBusError if this stream is in an erroneous state, NULL otherwise
     */
    COMMONAPI_EXPORT const DBusError &getError() const;

    /**
     * @return true if this stream is in an erroneous state, false otherwise.
     */
    COMMONAPI_EXPORT bool isErrorSet() const;

    // Marks the state of the stream as cleared from all errors. Further reading is possible afterwards.
    // The stream will have maintained the last valid position from before its state became erroneous.
    COMMONAPI_EXPORT void clearError();

    /**
     * Aligns the stream to the given byte boundary, i.e. the stream skips as many bytes as are necessary to execute the next read
     * starting from the given boundary.
     *
     * @param _boundary the byte boundary to which the stream needs to be aligned.
     */
    COMMONAPI_EXPORT void align(const size_t _boundary);

    /**
     * Reads the given number of bytes and returns them as an array of characters.
     *
     * Actually, for performance reasons this command only returns a pointer to the current position in the stream,
     * and then increases the position of this pointer by the number of bytes indicated by the given parameter.
     * It is the user's responsibility to actually use only the number of bytes he indicated he would use.
     * It is assumed the user knows what kind of value is stored next in the #DBusMessage the data is streamed from.
     * Using a reinterpret_cast on the returned pointer should then restore the original value.
     *
     * Example use case:
     * @code
     * ...
     * inputMessageStream.alignForBasicType(sizeof(int32_t));
     * char* const dataPtr = inputMessageStream.read(sizeof(int32_t));
     * int32_t val = *(reinterpret_cast<int32_t*>(dataPtr));
     * ...
     * @endcode
     */
    COMMONAPI_EXPORT char *_readRaw(const size_t _size);

    /**
     * Handles all reading of basic types from a given #DBusInputMessageStream.
     * Basic types in this context are: uint8_t, uint16_t, uint32_t, uint64_t, int8_t, int16_t, int32_t, int64_t, float, double.
     * Any types not listed here (especially all complex types, e.g. structs, unions etc.) need to provide a
     * specialized implementation of this operator.
     *
     * @tparam Type_ The type of the value that is to be read from the given stream.
     * @param _value The variable in which the retrieved value is to be stored
     * @return The given inputMessageStream to allow for successive reading
     */
    template<typename Type_>
    COMMONAPI_EXPORT DBusInputStream &_readValue(Type_ &_value) {
        if (sizeof(_value) > 1)
            align(sizeof(Type_));

        _value = *(reinterpret_cast<Type_ *>(_readRaw(sizeof(Type_))));

        return (*this);
    }

    COMMONAPI_EXPORT DBusInputStream &_readValue(float &_value) {
        align(sizeof(double));

        _value = (float) (*(reinterpret_cast<double*>(_readRaw(sizeof(double)))));
        return (*this);
    }

private:
    COMMONAPI_EXPORT void pushPosition();
    COMMONAPI_EXPORT size_t popPosition();

    COMMONAPI_EXPORT void pushSize(size_t _size);
    COMMONAPI_EXPORT size_t popSize();

    inline void skipSignature() {
        uint8_t length;
        _readValue(length);
        _readRaw(length + 1);
    }

    template<typename Type_>
    COMMONAPI_EXPORT void alignVector(typename std::enable_if<!std::is_class<Type_>::value>::type * = nullptr,
                     typename std::enable_if<!is_std_vector<Type_>::value>::type * = nullptr,
                     typename std::enable_if<!is_std_unordered_map<Type_>::value>::type * = nullptr) {
        if (4 < sizeof(Type_)) align(8);
    }

    template<typename Type_>
    COMMONAPI_EXPORT void alignVector(typename std::enable_if<!std::is_same<Type_, std::string>::value>::type * = nullptr,
                     typename std::enable_if<std::is_class<Type_>::value>::type * = nullptr,
                     typename std::enable_if<!is_std_vector<Type_>::value>::type * = nullptr,
                     typename std::enable_if<!is_std_unordered_map<Type_>::value>::type * = nullptr,
                     typename std::enable_if<!std::is_base_of<Enumeration<int32_t>, Type_>::value>::type * = nullptr) {
        align(8);
    }

    template<typename Type_>
    COMMONAPI_EXPORT void alignVector(typename std::enable_if<std::is_same<Type_, std::string>::value>::type * = nullptr) {
        // Intentionally do nothing
    }

    template<typename Type_>
    COMMONAPI_EXPORT void alignVector(typename std::enable_if<is_std_vector<Type_>::value>::type * = nullptr) {
        // Intentionally do nothing
    }

    template<typename Type_>
    COMMONAPI_EXPORT void alignVector(typename std::enable_if<is_std_unordered_map<Type_>::value>::type * = nullptr) {
        align(4);
    }

    template<typename Type_>
    COMMONAPI_EXPORT void alignVector(typename std::enable_if<std::is_base_of<Enumeration<int32_t>, Type_>::value>::type * = nullptr) {
        align(4);
    }

    char *begin_;
    size_t current_;
    size_t size_;
    CommonAPI::DBus::DBusError* exception_;
    CommonAPI::DBus::DBusMessage message_;

    std::stack<uint32_t> sizes_;
    std::stack<size_t> positions_;
};

} // namespace DBus
} // namespace CommonAPI

#endif // COMMONAPI_DBUS_DBUS_INPUTSTREAM_HPP_
