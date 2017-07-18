#include "utils.h"
#include <stdio.h>
#include <algorithm>
#include <boost/random/random_device.hpp>
#include <boost/random/uniform_int.hpp>
#include <boost/random/variate_generator.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include "logger.h"

const char *adverbs[] = {
    "adorable", "acidic",     "ample",        "aromatic",   "artistic", "attractive", "basic",    "beautiful",
    "best",     "blissful",   "bubbly",       "celebrated", "cheap",    "chilly",     "cloudy",   "colorful",
    "colossal", "complete",   "conventional", "costly",     "creamy",   "crisp",      "dense",    "double",
    "dry",      "easy",       "every",        "exotic",     "expert",   "fake",       "fabulous", "fast",
    "fine",     "firm",       "first",        "flaky",      "flat",     "fluffy",     "frozen",   "generous",
    "giant",    "glass",      "glorious",     "golden",     "good",     "grand",      "great",    "half",
    "happy",    "hard",       "healthy",      "heavy",      "hot",      "huge",       "humble",   "ideal",
    "icy",      "incredible", "interesting",  "joyous",     "juicy",    "jumbo",      "large",    "late",
    "lavish",   "leafy",      "lean",         "light",      "lovely",   "marvelous",  "mature",   "modern",
    "modest",   "neat",       "new",          "nice",       "nifty",    "nutty",      "oily",     "ornate",
    "perfect",  "plain",      "posh",         "pretty",     "prime",    "proper",     "pure",     "quick",
    "raw",      "real",       "rich",         "ripe",       "safe",     "salty",      "several",  "short",
    "simple",   "slim",       "slow",         "smooth",     "soft",     "solid",      "speedy",   "spotless",
    "strong",   "stylish",    "subtle",       "super",      "sweet",    "tasty",      "tempting", "tender",
    "terrific", "thick",      "thin",         "tidy",       "tiny",     "twin",       "ultimate", "unique",
    "uniform",  "unusual",    "valuable",     "vast",       "warm",     "wavy",       "wet",      "whole",
    "wide",     "wild",       "wooden",       "young"};

const char *names[] = {"Aal",
                       "Allerlei",
                       "Apfelkuchen",
                       "Baumkuchen",
                       "Beetenbartsch",
                       "Berliner",
                       "Birnen",
                       "Blutwurst",
                       "Bohnen",
                       "Botel",
                       "Bratkartoffeln",
                       "Bratwurst",
                       "Braunschweiger",
                       "Bregenwurst",
                       "Brotchen",
                       "Buletten",
                       "Butterkuchen",
                       "Creme",
                       "Currywurst",
                       "Dampfnudel",
                       "Dibbelabbes",
                       "Eierkuchen",
                       "Eisbein",
                       "Fischbrotchen",
                       "Fladlesuppe",
                       "Fleck",
                       "Frikadelle",
                       "Garnelen",
                       "Gedadschde",
                       "Grunkohl",
                       "Hackepeter",
                       "Hahn",
                       "Handkase",
                       "Hasenpfeffer",
                       "Hendl",
                       "Hochzeitssuppe",
                       "Karpfen",
                       "Kartoffelsalat",
                       "Kassler",
                       "Klopse",
                       "Kluntjes",
                       "Knipp",
                       "Knodel",
                       "Kohlroulade",
                       "Labskaus",
                       "Lebkuchen",
                       "Linsen",
                       "Marsch",
                       "Marzipan",
                       "Maultaschen",
                       "Mettbrotchen",
                       "Milliramstrudel",
                       "Muscheln",
                       "Pellkartoffeln",
                       "Pfannkuchen",
                       "Pfefferkuchen",
                       "Pichelsteiner",
                       "Pinkel",
                       "Pottwurst",
                       "Printen",
                       "Prinzregententorte",
                       "Quarkkeulchen",
                       "Radi",
                       "Rouladen",
                       "Sahnehering",
                       "Sauerbraten",
                       "Saumagen",
                       "Schlachtschussel",
                       "Schmalzkuchen",
                       "Schnusch",
                       "Schupfnudel",
                       "Schwarzsauer",
                       "Schweinshaxe",
                       "Schwenker",
                       "Spanferkel",
                       "Spargel",
                       "Spatzle",
                       "Speck",
                       "Spirgel",
                       "Springerle",
                       "Sprotten",
                       "Stollen",
                       "Tilsit",
                       "Topfenstrudel",
                       "Weihnachtsgans",
                       "Wibele",
                       "Zipfel",
                       "Zwiebelkuchen"};

std::string Utils::fromBase64(std::string base64_string) {
  unsigned long long paddingChars = std::count(base64_string.begin(), base64_string.end(), '=');
  std::replace(base64_string.begin(), base64_string.end(), '=', 'A');
  std::string result(base64_to_bin(base64_string.begin()), base64_to_bin(base64_string.end()));
  result.erase(result.end() - static_cast<unsigned int>(paddingChars), result.end());
  return result;
}

std::string Utils::toBase64(std::string tob64) {
  std::string b64sig(base64_text(tob64.begin()), base64_text(tob64.end()));
  b64sig.append((3 - tob64.length() % 3) % 3, '=');
  return b64sig;
}

Json::Value Utils::parseJSON(const std::string &json_str) {
  Json::Reader reader;
  Json::Value json_value;
  reader.parse(json_str, json_value);
  return json_value;
}

Json::Value Utils::parseJSONFile(const boost::filesystem::path &filename) {
  std::ifstream path_stream(filename.string().c_str());
  std::string content((std::istreambuf_iterator<char>(path_stream)), std::istreambuf_iterator<char>());
  return Utils::parseJSON(content);
}

std::string Utils::intToString(unsigned int val) {
  std::ostringstream ss;
  ss << val;
  return ss.str();
}

std::string Utils::genPrettyName() {
  boost::random::random_device urandom;

  boost::random::uniform_int_distribution<> adverbs_dist(0, (sizeof(adverbs) / sizeof(char *)) - 1);
  boost::random::uniform_int_distribution<> nouns_dist(0, (sizeof(names) / sizeof(char *)) - 1);
  boost::random::uniform_int_distribution<> digits(0, 999);
  std::stringstream pretty_name;
  pretty_name << adverbs[adverbs_dist(urandom)];
  pretty_name << "-";
  pretty_name << names[nouns_dist(urandom)];
  pretty_name << "-";
  pretty_name << digits(urandom);
  std::string res = pretty_name.str();
  std::transform(res.begin(), res.end(), res.begin(), ::tolower);
  return res;
}

std::string Utils::readFile(const std::string &filename) {
  std::ifstream path_stream(filename.c_str());
  std::string content((std::istreambuf_iterator<char>(path_stream)), std::istreambuf_iterator<char>());
  return content;
}

void Utils::writeFile(const std::string &filename, const std::string &content) {
  std::ofstream file(filename.c_str());
  file << content;
  file.close();
}

void Utils::writeFile(const std::string &filename, const Json::Value &content) {
  std::ofstream file(filename.c_str());
  file << content;
  file.close();
}

Json::Value Utils::getHardwareInfo() {
  char buffer[128];
  std::string result = "";
  FILE *pipe = popen("lshw -json", "r");
  if (!pipe) {
    LOGGER_LOG(LVL_warning, "Could not execute shell.");
    return "";
  }

  while (!feof(pipe)) {
    if (fgets(buffer, 128, pipe) != NULL) result += buffer;
  }

  int exit_code = pclose(pipe);
  if (exit_code) {
    LOGGER_LOG(LVL_warning, "Could not execute lshw (is it installed?).");
    return Json::Value();
  } else {
    return Utils::parseJSON(result);
  }
}

std::string Utils::randomUuid() {
  boost::random::random_device urandom;
  boost::uuids::basic_random_generator<boost::random::random_device> uuid_gen(urandom);
  return boost::uuids::to_string(uuid_gen());
}
