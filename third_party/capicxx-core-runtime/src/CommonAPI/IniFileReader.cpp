// Copyright (C) 2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <fstream>
#include <sstream>

#include <CommonAPI/IniFileReader.hpp>
#include <CommonAPI/Logger.hpp>
#include <CommonAPI/Utils.hpp>

namespace CommonAPI {

const std::map<std::string, std::string> &
IniFileReader::Section::getMappings() const {
    return mappings_;
}

std::string
IniFileReader::Section::getValue(const std::string &_key) const {
    auto it = mappings_.find(_key);
    if (it != mappings_.end()) {
        return it->second;
    }
    return ("");
}

bool
IniFileReader::load(const std::string &_path) {
    std::ifstream configStream(_path);
    if (configStream.is_open()) {
        //COMMONAPI_INFO("Loading ini file from ", _path);

        int lineCounter(0);
        std::string currentSectionName;
        std::shared_ptr<Section> currentSection;

        while (!configStream.eof()) {
            std::string line;
            std::getline(configStream, line);
            lineCounter++;

            trim(line);

            std::size_t start = line.find('[');
            if (start == 0) {
                std::size_t end = line.find(']');
                if (end != line.npos) {
                    currentSectionName = line.substr(++start, --end);
                    if (sections_.end() == sections_.find(currentSectionName)) {
                        currentSection = std::make_shared<Section>();
                        if (currentSection) {
                            sections_[currentSectionName] = currentSection;
                        }
                    } else {
                        COMMONAPI_ERROR("Double definition of section \'",
                                        currentSectionName,
                                        "\' ignoring definition (line ",
                                        lineCounter,
                                        ")");
                        currentSection = nullptr;
                    }
                } else {
                    COMMONAPI_ERROR("Missing \']\' in section definition (line ",
                                    lineCounter, ")");
                }
            } else if (currentSection) {
                std::size_t pos = line.find('=');
                if (pos != line.npos) {
                    std::string key = line.substr(0, pos);
                    trim(key);
                    if (currentSection->mappings_.end()
                        != currentSection->mappings_.find(key)) {
                        COMMONAPI_ERROR("Double definition for key \'",
                                        key,
                                        "'\' in section \'",
                                        currentSectionName,
                                        "\' (line ",
                                        lineCounter,
                                        ")");
                    } else {
                        std::string value = line.substr(pos+1);
                        trim(value);
                        currentSection->mappings_[key] = value;
                    }
                } else if (line.size() > 0) {
                    COMMONAPI_ERROR("Missing \'=\' in key=value definition (line ",
                                    lineCounter, ")");
                }
            }
        }
    }
    return true;
}

const std::map<std::string, std::shared_ptr<IniFileReader::Section>> &
IniFileReader::getSections() const {
    return sections_;
}

std::shared_ptr<IniFileReader::Section>
IniFileReader::getSection(const std::string &_name) const {
    std::shared_ptr<IniFileReader::Section> section;

    auto sectionIterator = sections_.find(_name);
    if (sectionIterator != sections_.end()) {
        section = sectionIterator->second;
    }

    return section;
}

} // namespace CommonAPI
