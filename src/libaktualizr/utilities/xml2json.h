#include <boost/optional.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <istream>

#include "json/json.h"

namespace xml2json {

static inline void addSubArray(Json::Value &d, const std::string &key, const Json::Value &arr) {
  if (arr.size() == 0) {
    return;
  } else if (arr.size() == 1) {
    d[key] = arr[0];
  } else {
    d[key] = arr;
  }
}

static const int MAX_DEPTH = 10;

static inline Json::Value treeJson(const boost::property_tree::ptree &tree, int depth = 0) {
  namespace bpt = boost::property_tree;

  if (depth > MAX_DEPTH) {
    throw std::runtime_error("parse error");
  }

  bool leaf = true;
  Json::Value output;

  struct {
    // used to collasce same-key children into lists
    std::string key;
    Json::Value list = Json::Value(Json::arrayValue);
  } cur;

  for (auto it = tree.ordered_begin(); it != tree.not_found(); it++) {
    const std::string &val = it->first;
    const bpt::ptree &subtree = it->second;
    leaf = false;

    // xml attributes
    if (val == "<xmlattr>") {
      for (const bpt::ptree::value_type &attr : subtree) {
        output[std::string("@") + attr.first] = attr.second.data();
      }
      continue;
    }

    if (cur.key == "") {
      cur.key = val;
    } else if (cur.key != val) {
      addSubArray(output, cur.key, cur.list);

      cur.key = val;
      cur.list = Json::Value(Json::arrayValue);
    }
    cur.list.append(treeJson(subtree, depth + 1));
  }

  if (cur.key != "") {
    addSubArray(output, cur.key, cur.list);
  }

  {
    auto val = tree.get_value_optional<std::string>();
    if (!!val && val.get() != "") {
      if (leaf) {
        // <e>c</e> -> { "e": "c" }
        return val.get();
      } else {
        // <e a=b>c</e> -> { "e": { "@a": "b", "#text": "c" } }
        output["#text"] = val.get();
      }
    }
  }

  return output;
}

static inline Json::Value xml2json(std::istream &is) {
  namespace bpt = boost::property_tree;

  try {
    bpt::ptree pt;
    bpt::read_xml(is, pt, bpt::xml_parser::trim_whitespace);

    if (pt.size() != 1) {
      throw std::runtime_error("parse error");
    }

    return treeJson(pt);
  } catch (std::exception &e) {
    throw std::runtime_error("parse error");
  }
}

}  // namespace xml2json
