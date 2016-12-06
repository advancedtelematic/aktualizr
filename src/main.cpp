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
#include <iostream>
#include <boost/program_options.hpp>

#include "logger.hpp"
#include "ymlcfg.hpp"

/*****************************************************************************/

namespace bpo = boost::program_options;

/*****************************************************************************/
int main(int argc, char *argv[]) 
{
   // create a return value
   int returnValue;

   // initialize the return value to zero (everything is OK)
   returnValue = 0;

   // initialize the logging framework
   logger_init();

   // read configuration file
   // TODO: use a commanline option for setting the filename
   //       and take care that this is done before the processing
   //       the loglevel commandline option
   ymlcfg_readFile("config.yml");

   // set up the commandline options
   try
   {
      // create a commandline options object
      bpo::options_description cmdl_description("CommandLine Options:");

      // create a easy-to-handle dictionary for commandline options
      bpo::options_description_easy_init cmdl_dictionary = cmdl_description.add_options();

      // add a entry to the dictionary
      cmdl_dictionary("help,h", "Help screen"); // --help are valid arguments for showing "Help screen"

      // add a loglevel commandline option
      // TODO use the enumeration from the logger.hpp to enable automatic range checking
      // this requires to overload operators << and >> as shown in the boost header  boost/log/trivial.hpp
      // desired result: bpo::value<loggerLevels_t>
      cmdl_dictionary("loglevel", bpo::value<int>(), "set log level 0-4 (trace, debug, warning, info, error)");

      // create a variables map
      bpo::variables_map cmdl_varMap;

      // store command line options in the variables map
      bpo::store(parse_command_line(argc, argv, cmdl_description), cmdl_varMap);
      // run all notify functions of the variables in the map
      bpo::notify(cmdl_varMap);

      // check for command line arguments by getting a occurrence counter
      // by now the variable map is only checked for help or h respectively
      if (cmdl_varMap.count("help") != 0)
      {
         // print the description off all known command line options
         std::cout << cmdl_description << '\n';
         LOGGER_LOG(LVL_debug, "boost command line option: --help detected.");
      }

      // check for loglevel
      if(cmdl_varMap.count("loglevel") != 0)
      {
         // write a log message
         LOGGER_LOG(LVL_debug, "boost command line option: loglevel detected." );

         // get the log level from command line option
         logger_setSeverity( static_cast<loggerLevels_t>(cmdl_varMap["loglevel"].as<int>()) );
      }
      else
      {
         // log if no command line option loglevl is used
         LOGGER_LOG(LVL_debug, "no commandline option 'loglevel' provided");
      }

   }
   catch (const bpo::error &ex)
   {
      // log boost error
      LOGGER_LOG(LVL_warning, "boost command line option error: " << ex.what());

      // print the error message to the standard output too, as the user provided
      // a non-supported commandline option
      std::cout << ex.what() << '\n';

      // set the returnValue, thereby ctest will recognize
      // that something went wrong
      returnValue = 1;
   }

   return returnValue;
}
