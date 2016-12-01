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

#include "log.hpp"

/*****************************************************************************/
using namespace std;

namespace bpo = boost::program_options;

namespace logtrv = boost::log::trivial;

/*****************************************************************************/
int main(int argc, char *argv[]) 
{
   // create a return value
   int returnValue;

   // initialize the return value to zero (everything is OK)
   returnValue = 0;

   // initialize the logging framework
   logger_init();

   // TODO: get log level from config file before overwriting it by using
   // log level from command line option

   // create a logging object
   boost::log::sources::severity_logger< logtrv::severity_level > logger;

   BOOST_LOG_SEV(logger, logtrv::info) << "Program started and logging initialized";
   BOOST_LOG_SEV(logger, logtrv::trace) << "Check command line options";

   // set up the commandline options
   try
   {
      // create a commandline options object
      bpo::options_description cmdl_description("CommandLine Options:");

      // create a easy-to-handle dictionary for commandline options
      bpo::options_description_easy_init cmdl_dictionary = cmdl_description.add_options();

      // add a entry to the dictionary
      cmdl_dictionary("help,h", "Help screen"); // --help are valid arguments for showing "Help screen"
      // add a loglevel commandline option using the severity_level enumeration provided by
      // boost log
      cmdl_dictionary("loglevel", bpo::value<logtrv::severity_level>(), "set log level (trace, debug, warning, info, error)");

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
         BOOST_LOG_SEV(logger, logtrv::trace) << "boost command line option: --help detected.";
      }

      // check for loglevel
      if(cmdl_varMap.count("loglevel") != 0)
      {
         // get the log level from command line option
         logtrv::severity_level loglvl = cmdl_varMap["loglevel"].as<logtrv::severity_level>();

         // check if the log level is in range
         if (loglvl <= logtrv::error)
         {
            // apply the log level
            logger_setSeverity( cmdl_varMap["loglevel"].as<logtrv::severity_level>() );
            // log the log level
            BOOST_LOG_SEV(logger, logtrv::info) << "Log level was set to " << loglvl << " by command line option 'loglevel'.";
         }
         else
         {
            // log out of range command line option for loglevel
            BOOST_LOG_SEV(logger, logtrv::info) << "Log level provided via"
                  "command line option 'loglevel' is out of range - " << loglvl << " was provided.";
         }
      }
      else
      {
         // log if no command line option loglevl is used
         BOOST_LOG_SEV(logger, logtrv::debug) << "no commandline option 'loglevel' provided";
      }

   }
   catch (const bpo::error &ex)
   {
      // log boost error
      BOOST_LOG_SEV(logger, logtrv::warning) << "boost command line option: " << ex.what() << '\n';
      std::cout << ex.what() << '\n';
      // set the returnValue, thereby ctest will recognize
      // that something went wrong
      returnValue = 1;
   }

   return returnValue;
}
