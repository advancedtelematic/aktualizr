
//*****************************************************************************
//
// Copyright (C) 2016 ATS Advanced Telematic Systems GmbH
//
//
// Module-Name: main.cpp
// Author: Moritz Klinger
//
//
//*****************************************************************************

#include <boost/program_options.hpp>
#include <iostream>

using namespace std;

namespace bpo = boost::program_options;

int main(int argc, char *argv[]) 
{
   // create a return value
   int returnValue;

   // initialize the return value to zero (everything is OK)
   returnValue = 0;

   // set up the commandline options
   try
   {
      // create a commandline options object
      bpo::options_description cmdl_description("CommandLine Options:");

      // create a easy-to-handle dictionary for commandline options
      bpo::options_description_easy_init cmdl_dictionary = cmdl_description.add_options();

      // add a entry to the dictionary
      cmdl_dictionary("help,h", "Help screen"); // --help or -h are valid arguments for showing "Help screen"

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
      }
   }
   catch (const bpo::error &ex)
   {
      // print boost error
      std::cerr << ex.what() << '\n';
      // set the returnValue, thereby ctest will recognize
      // that something went wrong
      returnValue = 1;
   }

   return returnValue;
}
