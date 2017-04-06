/* Copyright (c) 2016, Jaguar Land Rover. All Rights Reserved.
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0.
 */

#include <signal.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "rvi.h"

#define BUFSIZE 256
#define LEN 10

void callbackFunc(int fd, void *service_data, const char *parameters);

void waitFor(unsigned int secs);

void processChoice( int choice );
 
void listChoices(void); 

void smpl_initialize(void);

void smpl_connect(void);

void smpl_disconnect(void);

void get_connections(void);

void register_service(void);

void unregister_service(void);

void get_services(void);

void smpl_invoke(void);

void smpl_process(void);

void smpl_shutdown(void);

TRviHandle myHandle = {0};

int main(int argc, char *argv[])
{
    signal(SIGPIPE, SIG_IGN); // ignore broken pipes
    int choice;

    listChoices();

    while ( scanf("%d", &choice) ) {
        processChoice( choice );
        printf("Make a choice: ");
    }

    return 0;
}

void callbackFunc(int fd, void *service_data, const char *parameters)
{
    printf("inside the callback function, invoked by fd %d\n", fd);
    printf("received parameters: \n\t%s\n", parameters);
}

void waitFor(unsigned int secs) 
{
    unsigned int retTime = time(0) + secs;  /* Get end time */
    while( time(0) < retTime);              /* Loop until it arrives */
}

void processChoice( int choice ) 
{
    switch( choice ) {
        case 0:
            printf("You chose to initialize RVI.\n");
            smpl_initialize();
            break;
        case 1:
            printf("You chose to connect to a remote node.\n");
            smpl_connect();
            break;
        case 2:
            printf("Which RVI node would you like to disconnect from? ");
            smpl_disconnect();
            break;
        case 3:
            printf("You chose to get a list of connections.\n");
            get_connections();
            break;
        case 4:
            printf("You chose to register a service.\n");
            register_service();
            break;
        case 5:
            printf("You chose to unregister a service.\n");
            unregister_service();
            break;
        case 6:
            printf("You chose to get a list of services.\n");
            get_services();
            break;
        case 7:
            printf("What service would you like to invoke?\n");
            smpl_invoke();
            break;
        case 8:
            printf("You chose to process input.\n");
            smpl_process();
            break;
        case 9:
            printf("You chose to shutdown RVI.\n");
            smpl_shutdown();
            break;
        case 10:
            printf("Goodbye!\n");
            exit(0);
        default:
            printf("That's not a valid choice.\n");
            break;
    }
}

void listChoices(void) {
    printf("\t[0]: Initialize RVI\n"
           "\t[1]: Connect to remote node\n"
           "\t[2]: Disconnect from remote node\n"
           "\t[3]: Get a list of connections\n"
           "\t[4]: Register a service\n"
           "\t[5]: Unregister a service\n"
           "\t[6]: Get a list of services\n"
           "\t[7]: Invoke remote service\n"
           "\t[8]: Process input\n"
           "\t[9]: Shutdown RVI\n"
           "\t[10]: Close program.\n"
           "Make a choice: "
           );
}

void smpl_initialize(void)
{
    char input[BUFSIZE] = {0};
    printf("Config file: ");
    scanf("%s", input);
    myHandle = rviInit(input);
    if( !myHandle ) {
        printf("Failed to initialize RVI.\n");
        return;
    }
    printf("RVI initialized!\n");
}

void smpl_connect(void)
{
    if(!myHandle) {
        printf("Please initialize RVI first!\n");
        return;
    }

    char addr[BUFSIZE] = {0};
    char port[BUFSIZE] = {0};

    printf("OK, which IP? ");

    scanf("%s", addr);

    printf("Got it. Which port at IP %s? ", addr);

    scanf("%s", port);

    if( addr == NULL || port == NULL ) {
        printf("That's not a valid address.\n");
        return;
    }

    int stat = rviConnect(myHandle, addr, port);
    if( stat > 0 ) {
        printf("Connected to remote node on fd %d!\n", stat);
    } else {
        printf("Failed to connect to remote node.\n");
    }
}

void smpl_disconnect(void)
{
    if(!myHandle) {
        printf("Please initialize RVI first!\n");
        return;
    }
    int fd;
    scanf("%d", &fd);
    int stat = rviDisconnect(myHandle, fd);
    if( stat ) {
        printf("Error disconnecting from %d\n", fd);
    } else {
        printf("Disconnected from %d!\n", fd);
    }
}

void get_connections(void)
{
    if(!myHandle) {
        printf("Please initialize RVI first!\n");
        return;
    }
    int *connections = malloc( LEN * sizeof(int *) );
    int len = LEN;
    int *conn = connections;
    
    for(int i = 0; i < len; i++) {
        connections[i] = 0;
    }

    int stat = rviGetConnections(myHandle, connections, &len);

    if( stat ) { 
        printf("Didn't get any connections.\n");
        free(connections);
        return;
    }

    while(*connections != 0) {
        printf("\tconnection on fd %d\n", *connections++);
    }
    free(conn);
}

void register_service(void)
{
    if(!myHandle) {
        printf("Please initialize RVI first!\n");
        return;
    }
    char service[BUFSIZE] = {0};
    printf("What service would you like to register? ");
    scanf("%s", service);
    int stat = rviRegisterService(myHandle, service, callbackFunc, NULL);
    if( stat ) {
        printf("Couldn't register the service %s\n", service);
    } else {
        printf("Registered %s!\n", service);
    }
}

void unregister_service(void)
{
    if(!myHandle) {
        printf("Please initialize RVI first!\n");
        return;
    }
    char service[BUFSIZE] = {0};
    printf("What service would you like to unregister? ");
    scanf("%s", service);
    int stat = rviUnregisterService(myHandle, service);
    if( stat ) {
        printf("Couldn't unregister the service %s\n", service);
    } else {
        printf("Unregistered %s!\n", service);
    }
}

void get_services(void)
{
    if(!myHandle) {
        printf("Please initialize RVI first!\n");
        return;
    }
    int len = LEN;
    char **services = malloc( LEN * sizeof( char * ) );

    for(int i = 0; i < len; i++) {
        services[i] = NULL;
    }

    int stat = rviGetServices(myHandle, services, &len);
    if( stat ) {
        printf("Couldn't get any services.\n");
        free(services);
        return;
    }

    for(int i = 0; i < len; i++) {
        printf("\t%s\n", services[i]);
    }
            
    char **svcs = services;
    while(*services != NULL) {
        free(*services++);
    }
    free(svcs);
}

void smpl_invoke(void)
{
    if(!myHandle) {
        printf("Please initialize RVI first!\n");
        return;
    }
    char input[BUFSIZE] = {0};
    char params[BUFSIZE] = {0};

    scanf("%s", input);
    fflush(stdin);
    printf("Okay, you chose to invoke %s. Any parameters?\n"
                    "Please supply a JSON object: ", input);
    scanf("%s", params);
    int stat = rviInvokeService(myHandle, input, params);
    if( stat == RVI_ERR_JSON ) {
        printf( "Error: invalid JSON for parameters\n" );
    } else if ( stat ) {
        printf( "Error: couldn't invoke %s\n", input );
    } else {
        printf("Invoked %s!\n", input);
    }
}

void smpl_process(void)
{
    if(!myHandle) {
        printf("Please initialize RVI first!\n");
        return;
    }
    int len;
    printf("How many connections? ");
    scanf("%d*[^\n]", &len);
    
    if( len <= 0 ) {
        printf("Sorry, %d is not a valid number of connections.\n", len);
        return;
    }

    int *connections = malloc( len * sizeof(int *) );

    printf("Which connections? ");
    for(int i = 0; i < len; i++) {
        scanf("%d", &connections[i]);
    }

    int stat = rviProcessInput(myHandle, connections, len);

    free( connections );
    if (stat == RVI_ERR_JSON_PART) {
        printf("Data remaining to be read on one or more connections\n");
    } else if (stat) {
        printf("Couldn't process input on one or more connections...\n");
    } else {
        printf("All input processed!\n");
    }
}

void smpl_shutdown(void)
{
    if(!myHandle) {
        printf("Please initialize RVI first!\n");
        return;
    }
    int stat = rviCleanup(myHandle);
    if(stat == RVI_OK) {
        myHandle = NULL; 
        printf("Closed down RVI context!\n");
    } else {
        printf("Uh oh... an error occurred when shutting down RVI.\n");
    }
    
}
