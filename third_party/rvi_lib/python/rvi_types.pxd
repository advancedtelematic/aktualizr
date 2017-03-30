## Copyright (c) 2016, Jaguar Land Rover. All Rights Reserved.
# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, you can obtain one at http://mozilla.org/MPL/2.0.
##

# Cython declarations for rvi_lib
#
# -- Tatiana Jamison &lt;tjamison@jaguarlandrover.com&gt;

cdef extern from "rvi.h":

# Application handle used to interact with RVI 
    ctypedef void *TRviHandle

# Function signature for RVI callback functions
    ctypedef void (*TRviCallback) ( int fd, void* serviceData, 
                                    const char *parameters 
                                  )

# Function return status codes 
    ctypedef enum ERviStatus:
        RVI_OK                  = 0,    
        RVI_ERR_OPENSSL         = 100,  
        RVI_ERR_NOCONFIG        = 1001, 
        RVI_ERR_JSON            = 1002, 
        RVI_ERR_SERVCERT        = 1003, 
        RVI_ERR_CLIENTCERT      = 1004, 
        RVI_ERR_NORCVCERT       = 1005, 
        RVI_ERR_STREAMEND       = 1006, 
        RVI_ERR_NOCRED          = 1007, 
        RVI_ERR_NOCMD           = 1008, 
        RVI_ERR_RIGHTS          = 1009 

    TRviHandle rviInit(char *configFilename)
    int rviCleanup(TRviHandle handle)

    # Connect to a remote node at a specified address and port. 
    int rviConnect(TRviHandle handle, const char *addr, const char *port)

    # @brief Disconnect from a remote node with a specified file descriptor
    int rviDisconnect(TRviHandle handle, int fd)

    # @brief Return all file descriptors in the RVI context
    int rviGetConnections(TRviHandle handle, int *conn, int *connSize)

    # @brief Register a service with a callback function
    int rviRegisterService( TRviHandle handle, const char *serviceName, 
                                     TRviCallback callback, 
                                     void* serviceData )

    # @brief Unregister a previously registered service
    int rviUnregisterService( TRviHandle handle, const char *serviceName )

    # @brief Get list of services available
    int rviGetServices( TRviHandle handle, char **result, int* len )

    # @brief Invoke a remote service
    int rviInvokeService( TRviHandle handle, 
                                          const char *serviceName, 
                                          const char *parameters )

    # @brief Handle input on remote connection(s).
    int rviProcessInput(TRviHandle handle, int* fdArr, int fdLen)
