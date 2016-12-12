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
#include <iostream>

#include "logger.hpp"
#include "servercon.hpp"
#include "ymlcfg.hpp"

/*****************************************************************************/

namespace bpo = boost::program_options;

/*****************************************************************************/
int main(int argc, char *argv[]) {
  // create a return value
  int returnValue;

  // initialize the return value to zero (everything is OK)
  returnValue = 0;

  // create a servercon object
  sota_server::servercon Server;

  // initialize the logging framework
  logger_init();

  // create a commandline options object
  bpo::options_description cmdl_description("CommandLine Options");

  // set up the commandline options
  try {

    // create a easy-to-handle dictionary for commandline options
    bpo::options_description_easy_init cmdl_dictionary =
        cmdl_description.add_options();

    // add a entry to the dictionary
    cmdl_dictionary(
        "help,h",
        "Help screen");  // --help are valid arguments for showing "Help screen"

    // add a loglevel commandline option
    // TODO issue #15 use the enumeration from the logger.hpp to enable
    // automatic range checking
    // this requires to overload operators << and >> as shown in the boost
    // header  boost/log/trivial.hpp
    // desired result: bpo::value<loggerLevels_t>
    cmdl_dictionary("loglevel", bpo::value<int>(),
        "set log level 0-4 (trace, debug, warning, info, error)");

    cmdl_dictionary("config,c", bpo::value<std::string>()->required(),
        "yaml configuration file");

    // create a variables map
    bpo::variables_map cmdl_varMap;

    // store command line options in the variables map
    bpo::store(parse_command_line(argc, argv, cmdl_description), cmdl_varMap);
    // run all notify functions of the variables in the map
    bpo::notify(cmdl_varMap);

    // check for command line arguments by getting a occurrence counter
    // by now the variable map is only checked for help or h respectively
    if (cmdl_varMap.count("help") != 0) {
      // print the description off all known command line options
      std::cout << cmdl_description << '\n';
      LOGGER_LOG(LVL_debug, "boost command line option: --help detected.");
    }

    // check for config file commandline option
    // The config file has to be checked before checking for loglevel
    // as the loglevel option shall overwrite settings provided via
    // a configuration file.
    if (cmdl_varMap.count("config") != 0) {
      ymlcfg_readFile(cmdl_varMap["config"].as<std::string>());
    }

    // check for loglevel
    if (cmdl_varMap.count("loglevel") != 0) {
      // write a log message
      LOGGER_LOG(LVL_debug, "boost command line option: loglevel detected.");

      // get the log level from command line option
      logger_setSeverity(
          static_cast<loggerLevels_t>(cmdl_varMap["loglevel"].as<int>()));
    } else {
      // log if no command line option loglevl is used
      LOGGER_LOG(LVL_debug, "no commandline option 'loglevel' provided");
    }
  }
  // check for missing options that are marked as required
  catch(const bpo::required_option& ex)
  {
    if (ex.get_option_name() == "--config"){
        std::cout << ex.get_option_name() << " is missing.\nYou have to provide a valid configuration file using yaml format. See the example configuration file in config/config.yml.example" << std::endl;
    }
    else
    {
    // print the error and append the default commandline option description
    std::cout << ex.what() << std::endl
     << cmdl_description;
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
    return EXIT_FAILURE;
  }

  // apply configuration data and contact the server if data is available
  if (ymlcfg_setServerData(&Server) == 1u) {
    // try current functionality of the servercon class
    LOGGER_LOG(
        LVL_info,
        "main - try to get token: "
        << ((Server.get_oauthToken() == 1u) ? "success" : "fail"));
    returnValue = EXIT_SUCCESS;
  } else {
    LOGGER_LOG(LVL_warning, "no server data available");
  }

  return returnValue;
}
