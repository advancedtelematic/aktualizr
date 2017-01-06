// Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

//#define COMMONAPI_DBUS_DEBUG_SERIALIZATION

#ifdef COMMONAPI_DBUS_DEBUG_SERIALIZATION
#include <iomanip>
#include <sstream>
#endif

#include <CommonAPI/DBus/DBusOutputStream.hpp>

namespace CommonAPI {
namespace DBus {

DBusOutputStream::DBusOutputStream(DBusMessage dbusMessage)
    : dbusMessage_(dbusMessage) {
}

// Internal
size_t DBusOutputStream::getPosition() {
    return payload_.size();
}

void DBusOutputStream::pushPosition() {
    positions_.push(payload_.size());
}

size_t DBusOutputStream::popPosition() {
    size_t itsPosition = positions_.top();
    positions_.pop();
    return itsPosition;
}

// TODO Check where dbusError_ is set
bool DBusOutputStream::hasError() const {
    return dbusError_;
}

// Writes the data that was buffered within this #DBusOutputMessageStream to
// the #DBusMessage that was given to the constructor. Each call to flush()
// will completely override the data that currently is contained in the
// #DBusMessage. The data that is buffered in this #DBusOutputMessageStream
// is not deleted by calling flush().
void DBusOutputStream::flush() {
    const std::string::size_type toWrite = payload_.size();
    dbusMessage_.setBodyLength(static_cast<int>(toWrite));
    char* destinationDataPtr = dbusMessage_.getBodyData();
    memcpy(destinationDataPtr, payload_.c_str(), toWrite);
}

void DBusOutputStream::setError() {
}

/**
 * Reserves the given number of bytes for writing, thereby negating the need to
 * dynamically allocate memory while writing. Use this method for optimization:
 * If possible, reserve as many bytes as you need for your data before doing
 * any writing.
 *
 * @param numOfBytes The number of bytes that should be reserved for writing.
 */
void DBusOutputStream::reserveMemory(size_t numOfBytes) {
    assert(numOfBytes > 0);
    payload_.reserve(numOfBytes);
}

DBusOutputStream& DBusOutputStream::writeString(const char *_value, const uint32_t &_length) {
    assert(_value != NULL);
    assert(_value[_length] == '\0');

    _writeValue(_length);
    _writeRaw(_value, _length + 1);

    return (*this);
}

// Additional 0-termination, so this is 8 byte of \0
static const char eightByteZeroString[] = "\0\0\0\0\0\0\0";

void DBusOutputStream::align(const size_t _boundary) {
    assert(_boundary > 0 && _boundary <= 8 &&
        (_boundary % 2 == 0 || _boundary == 1));

    size_t mask = _boundary - 1;
    size_t necessary = ((mask - (payload_.size() & mask)) + 1) & mask;

    _writeRaw(eightByteZeroString, necessary);
}

void DBusOutputStream::_writeRaw(const char *_data, const size_t _size) {
    payload_.append(_data, _size);
}

void DBusOutputStream::_writeRawAt(const char *_data, const size_t _size, size_t _position) {
    payload_ = payload_.replace(_position, _size, _data, _size);
}

void DBusOutputStream::writeSignature(const std::string& signature) {
    const auto& signatureLength = signature.length();
    assert(signatureLength > 0 && signatureLength < 256);

    const uint8_t wireLength = (uint8_t) signatureLength;
    (*this) << wireLength;
    _writeRaw(signature.c_str(), wireLength + 1);
}

} // namespace DBus
} // namespace CommonAPI
