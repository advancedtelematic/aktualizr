## Copyright (c) 2016, Jaguar Land Rover. All Rights Reserved.
# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, you can obtain one at http://mozilla.org/MPL/2.0.
##

## 
# @author Tatiana Jamison &lt;tjamison@jaguarlandrover.com&gt;
##

DEF BUFSIZE = 25

cimport rvi_types

cdef extern from "stdlib.h":
    void *malloc(size_t size)
    void free(void *ptr)

cdef void rvi_callback( int fd, void *serviceData, const char *parameters ):
    "Callback function that can be passed as TRviCallback"
    try:
        # recover Python function object from void* argument
        func = <object>serviceData
        # call function
        func(parameters)
    except:
        # catch any Python errors and return error indicator
        raise RuntimeError("an error occurred")

cdef class RVIService:
    cdef object _fqsn
    cdef object _callback
    def __cinit__(self, fqsn, callback):
        self._fqsn = fqsn
        if self._fqsn is None:
            raise MemoryError()
        self._callback = callback
        if self._callback is None:
            raise MemoryError()

cdef class RVI:
    cdef rvi_types.TRviHandle _thisptr

    def __cinit__(self, char *configFilename):
        self._thisptr = rvi_types.rviInit(configFilename)
        if self._thisptr == NULL:
            msg = "Configuration error."
            raise Exception(msg)
    def __dealloc__(self):
        if self._thisptr != NULL:
            rvi_types.rviCleanup(self._thisptr)

    def process_input(self, fds: list):
        cdef:
            int *conn;
            unsigned int i
            int size

        size = len(fds)
        if size <= 0:
            raise ValueError("Not a valid array")
        # Allocate the C array
        conn = <int *>malloc( size * sizeof(int))
        if conn == NULL:
            raise MemoryError("Unable to allocate array.")

        # Fill the C array with the specified values, coerced to ints
        for i in range(size):
            conn[i] = <int?>fds[i]

        # Get the status
        stat = rvi_types.rviProcessInput(self._thisptr, conn, size)

        # Free the C array
        free(conn)
        
        if stat != 0:
            raise Exception("Unable to process input...")

    def connect(self, const char *addr, const char *port):
        stat = rvi_types.rviConnect(self._thisptr, addr, port)
        if stat <= 0:
            raise Exception("Unable to connect.")
        return stat

    def disconnect(self, int fd):
        stat = rvi_types.rviDisconnect(self._thisptr, fd)
        if stat != 0:
            raise Exception("Unable to disconnect.")

    def get_connections(self):
        result = []
        cdef: 
            int len, i
            int *conn

        len = BUFSIZE

        # Allocate C array
        conn = <int *>malloc(BUFSIZE * sizeof( int *) )
        if conn == NULL:
            raise MemoryError("Unable to allocate array.")

        # Initialize C array
        for i in range(len):
            conn[i] = 0;

        stat = rvi_types.rviGetConnections(self._thisptr, conn, &len)

        if stat != 0:
            raise Exception("Error retrieving connections.")

        for i in range(len):
            result.append(conn[i])

        free(conn)

        return result

    def register_service(self, RVIService service):
        rvi_types.rviRegisterService(self._thisptr, service._fqsn, 
                            rvi_callback, <void *> service._callback );
    
    def unregister_service(self, const char *serviceName):
        return rvi_types.rviUnregisterService(self._thisptr, serviceName)

    def get_services(self):
        result = []
        cdef:
            int len, i
            char **services
            
        len = BUFSIZE
        services = <char **>malloc ( BUFSIZE * sizeof( char* ) )
        if services == NULL:
            raise MemoryError("Unable to allocate array.")

        for i in range(len):  
            services[i] = NULL 

        rvi_types.rviGetServices(self._thisptr, services, &len)

        for i in range(len):
            result.append(services[i])
            free(services[i])

        free(services)

        return result

    def invoke_service(self, const char *serviceName, 
                        const char *parameters = NULL 
                       ):
        """Expects a properly-stringified form of a JSON object, e.g.:
        >>> obj = {'hello':'world'}
        >>> params = json.dumps(obj)
        >>> rvi.invoke_service("ExampleServiceName", params)
        """
        if( parameters != NULL):
            print(parameters)
        return rvi_types.rviInvokeService(self._thisptr, 
                                          serviceName, parameters)
