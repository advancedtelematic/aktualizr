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


/**
 * \par Description:
 *    Reads a yaml configuration file and stores values.
 *
 * param[in] filename - the file that contains the configuration encoded with yaml
 */
extern void ymlcfg_readFile(const std::string& filename);
