/*!
 * \cond FILEINFO
 ******************************************************************************
 * \file ymlcfg.hpp
 ******************************************************************************
 *
 * Copyright (C) ATS Advanced Telematic Systems GmbH GmbH, 2016
 *
 * \author Moritz Klinger
 *
 ******************************************************************************
 *
 * \brief  Process configuration encoded in yml (yaml) files.
 *
 *
 ******************************************************************************
 *
 * \endcond
 */

#ifndef YMLCFG_H_
#define YMLCFG_H_

#include "servercon.hpp"

/**
 * \par Description:
 *    Reads a yaml configuration file and stores values.
 *
 * param[in] filename - the file that contains the configuration encoded with
 * yaml
 */
extern void ymlcfg_readFile(const std::string& filename);

/**
 * \par Description:
 *    Sets server-data from the configfile in a servercon class.
 *
 * \param[in] sota_serverPtr - Pointer to a servercon object
 */
unsigned int ymlcfg_setServerData(sota_server::servercon* sota_serverPtr);

#endif  // YMLCFG_H_
