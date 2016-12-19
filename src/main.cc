/*!
 * \cond FILEINFO
 ******************************************************************************
 * \file main.cpp
 ******************************************************************************
 *
 * Copyright (C) ATS Advanced Telematic Systems GmbH GmbH, 2016
 *
 * \author Moritz Klinger
 *
 ******************************************************************************
 *
 * \brief  The main file of the project.
 *
 *
 ******************************************************************************
 *
 * \endcond
 */

/*****************************************************************************/
#include <boost/program_options.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include "config.h"
#include "logger.h"

#include "servercon.h"

/*****************************************************************************/

namespace bpo = boost::program_options;

/*****************************************************************************/
int main(int argc, char *argv[]) {
  // create and initialize the return value to zero (everything is OK)
  int return_value = EXIT_SUCCESS;

  loggerInit();

  // Initialize config with default values
  Config config;

  // create a commandline options object
  bpo::options_description commandline_description("CommandLine Options");

  // set up the commandline options
  try {
    // create a easy-to-handle dictionary for commandline options
    bpo::options_description_easy_init commandline_dictionary =
        commandline_description.add_options();

    // add a entry to the dictionary
    commandline_dictionary(
        "help,h",
        "Help screen");  // --help are valid arguments for showing "Help screen"

    // add a loglevel commandline option
    // TODO issue #15 use the enumeration from the logger.hpp to enable
    // automatic range checking
    // this requires to overload operators << and >> as shown in the boost
    // header  boost/log/trivial.hpp
    // desired result: bpo::value<loggerLevels_t>
    commandline_dictionary(
        "loglevel", bpo::value<int>(),
        "set log level 0-4 (trace, debug, warning, info, error)");

    commandline_dictionary("config,c", bpo::value<std::string>()->required(),
                           "yaml configuration file");

    // create a variables map
    bpo::variables_map commandline_var_map;

    // store command line options in the variables map
    bpo::store(parse_command_line(argc, argv, commandline_description),
               commandline_var_map);
    // run all notify functions of the variables in the map
    bpo::notify(commandline_var_map);
    // check for command line arguments by getting a occurrence counter
    // by now the variable map is only checked for help or h respectively
    if (commandline_var_map.count("help") != 0) {
      // print the description off all known command line options
      std::cout << commandline_description << '\n';
      LOGGER_LOG(LVL_debug, "boost command line option: --help detected.");
    }

    // check for config file commandline option
    // The config file has to be checked before checking for loglevel
    // as the loglevel option shall overwrite settings provided via
    // a configuration file.
    if (commandline_var_map.count("config") != 0) {
      std::string filename = commandline_var_map["config"].as<std::string>();
      try {
        config.updateFromToml(filename);
      } catch (boost::property_tree::ini_parser_error e) {
        LOGGER_LOG(LVL_error, "Exception was thrown while parsing "
                                  << filename
                                  << " config file, message: " << e.message());
      }
    }

    // check for loglevel
    if (commandline_var_map.count("loglevel") != 0) {
      // write a log message
      LOGGER_LOG(LVL_debug, "boost command line option: loglevel detected.");

      // get the log level from command line option
      loggerSetSeverity(LVL_trace);
    } else {
      // log if no command line option loglevl is used
      LOGGER_LOG(LVL_debug, "no commandline option 'loglevel' provided");
    }
  }
  // check for missing options that are marked as required
  catch (const bpo::required_option &ex) {
    if (ex.get_option_name() == "--config") {
      std::cout << ex.get_option_name()
                << " is missing.\nYou have to provide a valid configuration "
                   "file using yaml format. See the example configuration file "
                   "in config/config.yml.example"
                << std::endl;
    } else {
      // print the error and append the default commandline option description
      std::cout << ex.what() << std::endl << commandline_description;
    }
    return EXIT_FAILURE;
  }
  // check for out of range options
  catch (const bpo::error &ex) {
    // log boost error
    LOGGER_LOG(LVL_warning, "boost command line option error: " << ex.what());

    // print the error message to the standard output too, as the user provided
    // a non-supported commandline option
    std::cout << ex.what() << '\n';

    // set the returnValue, thereby ctest will recognize
    // that something went wrong
    return_value = EXIT_FAILURE;
  }

  sota_server::ServerCon server(config);

  // try current functionality of the servercon class
  LOGGER_LOG(LVL_info,
             "main - try to get token: "
                 << ((server.getOAuthToken() == 1u) ? "success" : "fail"));

  return return_value;
}
