// Copyright (C) 2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef COMMONAPI_INIFILEREADER_HPP_
#define COMMONAPI_INIFILEREADER_HPP_

#include <map>
#include <memory>
#include <string>

#include <CommonAPI/Export.hpp>

namespace CommonAPI {

class IniFileReader {
public:
    class Section {
    public:
        COMMONAPI_EXPORT const std::map<std::string, std::string> &getMappings() const;
        COMMONAPI_EXPORT std::string getValue(const std::string &_key) const;
    private:
        std::map<std::string, std::string> mappings_;

    friend class IniFileReader;
    };

    COMMONAPI_EXPORT bool load(const std::string &_path);

    COMMONAPI_EXPORT const std::map<std::string, std::shared_ptr<Section>> &getSections() const;
    COMMONAPI_EXPORT std::shared_ptr<Section> getSection(const std::string &_name) const;

private:
    std::map<std::string, std::shared_ptr<Section>> sections_;
};

} // namespace CommonAPI

#endif // COMMONAPI_INIFILEREADER_HPP_
