/* Copyright (c) 2016, Jaguar Land Rover. All Rights Reserved.
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0. 
 */

/** @file rvi.c
 *
 * @author Tatiana Jamison &lt;tjamison@jaguarlandrover.com&gt;
 */

#include "rvi_list.h"
#include "btree.h"

#include <jansson.h>
#include <jwt.h>

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>

#include "rvi.h"
#include <dirent.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <time.h>

#define TLS_BUFSIZE  16384 /* Maximum TLS frame size is 16K bytes */

/* *************** */
/* DATA STRUCTURES */
/* *************** */

/** @brief verbose variable */
bool verbose = false;

/** @brief RVI context */
typedef struct TRviContext {

    /* 
     * Btrees for indexing remote connections, RVI services by name, and RVI 
     * services by registrant. 
     */
    btree_t *remoteIdx;        /* Remote connections by fd */
    btree_t *serviceNameIdx;  /* Services by fully qualified service name */
    btree_t *serviceRegIdx;   /* Services by fd of registering node ---*/
                            /*  note: local services designated 0 (stdin)  */

    /* Properties set in configuration file */
    char *cadir;    /* Directory containing the trusted certificate store */
    char *creddir;  /* Directory containing base64-encoded JWT credentials */
    char *certfile; /* File containing X.509 public key certificate (PKC) */
    char *keyfile;  /* File containing corresponding private key */
    char *cafile;   /* File containing CA public key certificate(s) */
    char *id;       /* Unique device ID. Format is "domain/type/uuid", e.g.: */
                    /* genivi.org/node/839fc839-5fab-472f-87b3-a8fbbd7e3935 */

    /* List of RVI credentials loaded into memory for quick access when
     * negotiating connections */
    TRviList *creds;

    /* SSL context for spawning new sessions.  */
    /* Contains X509 certs, config settings, etc */
    SSL_CTX *sslCtx;

    TRviList *rights;
} TRviContext;

/** @brief Data for connection to remote node */
typedef struct TRviRemote {
    /** File descriptor for the connection */
    int fd;
    /** List of TRviRights structures, containing receive & invoke rights and
     * expiration */
    TRviList *rights;
    /** Pointer to data buffer for partial I/O operations */
    void *buf;
    /** Length of buffer */
    int buflen;
    /** Pointer to BIO chain from OpenSSL library */
    BIO *sbio;
} TRviRemote;

/** @brief Data for service */
typedef struct TRviService {
    /** The fully-qualified service name */
    char *name;
    /** File descriptor of remote node that registered service */
    int registrant;
    /** Callback function to execute upon service invocation */
    TRviCallback callback;
    /** Service data to be passed to the callback */
    void *data;
} TRviService;

/** Data structure for rights parsed from validated credential */
typedef struct TRviRights {
    json_t *receive;    /* json array for right(s) to receive */
    json_t *invoke;     /* json array for right(s) to invoke */
    long expiration;     /* unix epoch time for jwt's validity.end */
} TRviRights;

/* 
 * Declarations for internal functions not exposed in the API 
 */

TRviService *rviServiceCreate ( const char *name, const int registrant, 
                                    const TRviCallback callback, 
                                    const void *serviceData );

void rviServiceDestroy ( TRviService *service );

TRviRemote *rviRemoteCreate ( BIO *sbio, const int fd );

void rviRemoteDestroy ( TRviRemote *remote );

TRviRights *rviRightsCreate (   const char *rightToReceive, 
                                    const char *rightToInvoke, 
                                    long validity );

void rviRightsDestroy ( TRviRights *rights );

void rviRightsListDestroy ( TRviList *list );

void rviCredentialListDestroy ( TRviList *list );

char *rviFqsnGet( TRviHandle handle, const char *serviceName );

/* Comparison functions for constructing btrees and retrieving values */
int rviCompareFd ( void *a, void *b );

int rviCompareRegistrant ( void *a, void *b );

int rviCompareName ( void *a, void *b );

int rviComparePattern ( const char *pattern, const char *fqsn );

/* Utility functions related to OpenSSL library */
int sslVerifyCallback ( int ok, X509_STORE_CTX *store );

SSL_CTX *rviSetupClientCtx ( TRviHandle handle );

/* Additional utility functions */

int rviReadJsonConfig ( TRviHandle handle, const char * filename );

int rviReadJsonChunk( json_t **json, char *src, TRviRemote *remote);

json_t *rviGetJsonGrant ( jwt_t *jwt, const char *grant );

char *rviGetPubkeyFile( char *filename );

int rviValidateCredential( TRviHandle handle, const char *cred, X509 *cert );

int rviGetRightsFromCredential( TRviHandle handle, const char *cred, 
                           TRviList *rights );

int rviRightToReceiveError( TRviList *rlist, const char *serviceName );

int rviRightToInvokeError( TRviList *rlist, const char *serviceName );

int rviRemoveService(TRviHandle handle, const char *serviceName);

int rviReadAu( TRviHandle handle, json_t *msg, TRviRemote *remote );

int rviWriteAu( TRviHandle handle, TRviRemote *remote );

int rviReadSa( TRviHandle handle, json_t *msg, TRviRemote *remote );

int rviAllServiceAnnounce( TRviHandle handle, TRviRemote *remote );

int rviServiceAnnounce( TRviHandle handle, TRviService *service, int available );

int rviReadRcv( TRviHandle handle, json_t *msg, TRviRemote *remote );

/****************************************************************************/

/* 
 * This function compares 2 pointers to TRviRemote structures on the basis of
 * the remote's file descriptor. It is used for building the index for remote
 * connections. 
 */
int rviCompareFd ( void *a, void *b )
{
    TRviRemote *remoteA = a;
    TRviRemote *remoteB = b;

    return ( remoteA->fd - remoteB->fd );
}

/* 
 * This function compares 2 pointers to TRviService structures on the basis
 * of the registrant (i.e., file descriptor). For services registered by the
 * same remote RVI node, the service name is used to guarantee a unique
 * position in the b-tree. 
 */
int rviCompareRegistrant ( void *a, void *b )
{
    TRviService *serviceA = a;
    TRviService *serviceB = b;

    int result;
    if ((( result = ( serviceA->registrant - serviceB->registrant)) == 0 ) &&
            serviceA->name && serviceB->name ) {
        result = strcmp ( serviceA->name, serviceB->name );
    }

    return result;
}

/* 
 * This function will compare 2 pointers to TRviService structures on the
 * basis of the unique fully-qualified service name. 
 */
int rviCompareName ( void *a, void *b )
{
    TRviService *serviceA = a;
    TRviService *serviceB = b;

    return strcmp ( serviceA->name, serviceB->name );
}

/*
 * This function compares an RVI pattern to a fully-qualified service name. If
 * the service name matches the pattern, it returns RVI_OK or an error
 * otherwise.
 */
int rviComparePattern ( const char *pattern, const char *fqsn )
{
    /* Check input */
    if( !pattern || !fqsn ) { return EINVAL; }
    /* While there are bytes to compare */
    while( *pattern != '\0' && *fqsn != '\0' ) {
        /* If there's a topic wildcard... */
        if( *pattern == '+' ) {
            /* Advance topic in pattern */
            while( *pattern++ != '/' && *pattern != '\0' );
            /* Advance topic in fqsn */
            while( *fqsn++ != '/' && *fqsn != '\0' );
        }
        /* If the bytes don't match, return error */
        if( *pattern++ != *fqsn++ ) { return -1; }
    }
    /* If the pattern still has characters, the fqsn doesn't match */
    if( *pattern != '\0' ) { return -1; }

    /* Otherwise, the fqsn matches the pattern */
    return RVI_OK;
}

/* 
 * This function initializes a new service struct and sets the name,
 * registrant, and callback to the specified values. 
 *
 * If service name is null or registrant is negative, this returns NULL and
 * performs no operations. 
 */

TRviService *rviServiceCreate ( const char *name, const int registrant, 
                                    const TRviCallback callback, 
                                    const void *serviceData )
{
    /* If name is NULL or registrant is negative, there's an error */
    if ( !name || (registrant < 0) ) { return NULL; }

    /* Zero-initialize the struct */
    TRviService *service = malloc( sizeof ( TRviService ) );
    if( !service ) { return NULL; }
    memset(service, 0, sizeof ( TRviService ) );

    /* Set the service name */
    service->name = strdup ( name );

    /* Set the service registrant */
    service->registrant = registrant;

    /* Set the callback. NULL is valid. */
    service->callback = callback;

    /* Set the serviceData. NULL is valid. */
    service->data = serviceData;

    /* Return the address of the new service */
    return service;
}

/* 
 * This function frees all memory allocated by a service struct. 
 * 
 * If service is null, no operations are performed. 
 */
void rviServiceDestroy ( TRviService *service )
{
     if ( !service ) { return; }

     free ( service->name );
     free ( service );
}

/*  
 * This function initializes a new remote struct and sets the file descriptor
 * and BIO chain to the specified values. 
 * 
 * If sbio is null or fd is negative, this returns NULL and performs no
 * operations. 
 */

TRviRemote *rviRemoteCreate ( BIO *sbio, const int fd )
{
    /* If sbio is null or fd is negative, there's a problem */
    if ( !sbio || fd < 0 ) { return NULL; }
    
    /* Create a new data structure and zero-initialize it */
    TRviRemote *remote = malloc ( sizeof ( TRviRemote ) );
    if( !remote ) { return NULL; }
    memset ( remote, 0, sizeof ( TRviRemote ) );

    /* Set the file descriptor and BIO chain */
    remote->fd = fd;
    remote->sbio = sbio;

    /* Note that we do NOT need to populate rightToReceive or 
     * rightToInvoke at this time. Those will be populated by parsing the au 
     * message. */
    remote->rights = malloc( sizeof( TRviList ) );
    if( !remote->rights ) { free( remote ); return NULL; }
    rviListInitialize( remote->rights );

    return remote;
}

/* 
 * This function frees all memory allocated by a remote struct. 
 * 
 * If remote is null, no operations are performed. 
 */
void rviRemoteDestroy ( TRviRemote *remote)
{
    if ( !remote ) { return; }

    rviRightsListDestroy( remote->rights );

    BIO_free_all ( remote->sbio );

    if ( remote->buflen ) free ( remote->buf );
    free ( remote );
}

/* This function creates a new rights struct for the given rights and
 * expiration */
TRviRights *rviRightsCreate (   const char *rightToReceive, 
                                    const char *rightToInvoke, 
                                    long validity )
{
    if( !rightToReceive || !rightToInvoke || validity < 1 ) {
        return NULL;
    }

    TRviRights *new = NULL;
    new = malloc( sizeof( TRviRights ) );
    if( !new ) { return NULL; }
    new->receive = json_loads( rightToReceive, 0, NULL);
    new->invoke = json_loads( rightToInvoke, 0, NULL);
    new->expiration = validity;

    return new;
}

/* This function destroys a rights struct and frees all allocated memory */
void rviRightsDestroy ( TRviRights *rights ) 
{
    if( !rights ) { return; }
    json_decref( rights->receive );
    json_decref( rights->invoke );
    free( rights );
}

/* This function destroys a list containing rights structures and frees all
 * allocated memory */
void rviRightsListDestroy ( TRviList *list )
{
    if( !list ) { return; }
    TRviListEntry *ptr = list->listHead;
    TRviListEntry *tmp;
    TRviRights *rights = NULL;
    while( ptr ) {
        tmp = ptr;
        rights = (TRviRights *)ptr->pointer;
        rviRightsDestroy( rights );
        ptr = ptr->next;
        free( tmp );
    }
    free( list );
}

void rviCredentialListDestroy ( TRviList *list )
{
    if( !list ) { return; }
    TRviListEntry *ptr = list->listHead;
    TRviListEntry *tmp;
    while( ptr ) {
        tmp = ptr;
        free( ptr->pointer );
        ptr = ptr->next;
        free( tmp );
    }
    free( list );
}

/* This returns a new buffer with the fully-qualified service name using this
 * node's own ID. The memory must be freed by the calling function. Returns
 * NULL on failure. */
char *rviFqsnGet( TRviHandle handle, const char *serviceName )
{
    if( !handle || !serviceName ) { return NULL; }

    TRviContext   *ctx    = (TRviContext *)handle;
    char            *fqsn   = NULL;

    size_t idlen = strlen( ctx->id );
    if( strncmp( serviceName, ctx->id, idlen ) != 0 ) {
        size_t namelen = strlen( serviceName );
        fqsn = malloc( namelen + idlen + 2 );
        if( !fqsn ) { return NULL; }
        sprintf( fqsn, "%s/%s", ctx->id, serviceName );
    } else {
        fqsn = strdup( serviceName );
    }

    return fqsn;
}

/* *************************** */
/* INITIALIZATION AND TEARDOWN */
/* *************************** */


int sslVerifyCallback ( int ok, X509_STORE_CTX *store )
{
    char data[256];

    if(!ok) {
        X509 *cert = X509_STORE_CTX_get_current_cert(store);
        int depth = X509_STORE_CTX_get_error_depth(store);
        int err = X509_STORE_CTX_get_error(store);

        fprintf(stderr, "-Error with certificate at depth: %i\n", depth);
        X509_NAME_oneline(X509_get_issuer_name(cert), data, 256);
        fprintf(stderr, " issuer = %s\n", data);
        X509_NAME_oneline(X509_get_subject_name(cert), data, 256);
        fprintf(stderr, " subject = %s\n", data);
        fprintf(stderr, " err %i:%s\n", err, 
                X509_verify_cert_error_string(err));
    }

    return ok;
}

/* 
 * Set up the SSL context. Configure for outbound connections only. 
 */
SSL_CTX *rviSetupClientCtx ( TRviHandle handle )
{
    if ( !handle ) { return NULL; }

    TRviContext *rviCtx = (TRviContext *)handle;

    SSL_CTX *sslCtx;

    /* Use generic SSL/TLS so we can easily add additional future protocols */
    sslCtx = SSL_CTX_new( SSLv23_method() );
    /* Do not permit the deprecated SSLv2 or SSLv3 to be used. Also prohibit 
     * TLSv1.0 and TLSv1.1. */
    SSL_CTX_set_options( sslCtx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 |
                             SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1 );

    /* Specify winnowed cipher list here */
    const char *cipherList = "HIGH";
    if(SSL_CTX_set_cipher_list(sslCtx, cipherList) != 1) { 
        SSL_CTX_free( sslCtx );
        return NULL; 
    } 

    if( SSL_CTX_load_verify_locations(sslCtx, rviCtx->cafile, 
                                      rviCtx->cadir) != 1 ) {
        SSL_CTX_free( sslCtx );
        return NULL;
    }
    if( SSL_CTX_set_default_verify_paths(sslCtx) != 1 ) {
        SSL_CTX_free( sslCtx );
        return NULL;
    }
    if( SSL_CTX_use_certificate_chain_file(sslCtx, 
                                           rviCtx->certfile) != 1 ) {
        SSL_CTX_free( sslCtx );
        return NULL;
    }
    if( SSL_CTX_use_PrivateKey_file(sslCtx, rviCtx->keyfile, 
                                    SSL_FILETYPE_PEM) != 1 ) {
        SSL_CTX_free( sslCtx );
        return NULL;
    }

    /* Set internal callback for peer verification on SSL connection attempts.
     */
    SSL_CTX_set_verify ( sslCtx, SSL_VERIFY_PEER, sslVerifyCallback );

    /* Set the maximum depth for certificates to be used. Additional
     * certificates are ignored. Error messages will be generated as if the
     * certificates are not present. 
     * 
     * Permits a maximum of 4 CA certificates, i.e., 3 intermediate CAs and the
     * root CA. 
     */
    SSL_CTX_set_verify_depth ( sslCtx, 4 );

    /* Ensure that SSL does not read ahead */
    SSL_CTX_set_read_ahead( sslCtx, 0);

    return sslCtx;
}
 
/**
 * This function will parse a JSON configuration file to retrieve the filenames
 * for the device certificate and key, as well as the directory names for CA
 * certificates and RVI credentials.
 *
 * On success, this function returns 0. On error, it will return a positive
 * error code.
 */
int rviParseJsonConfig ( TRviHandle handle, json_t *conf )
{
    if ( !handle || !conf ) { return EINVAL; }

    int             err         = RVI_OK;
    json_error_t    errjson;
    json_t          *tmp        = NULL;
    DIR             *d          = NULL;
    struct dirent   *dir        = {0};
    FILE            *fp         = NULL;
    TRviContext   *ctx        = (TRviContext *)handle;
    BIO             *certbio    = NULL;
    X509            *cert       = NULL;
    char            *cred       = NULL;

    tmp = json_object_get( conf, "dev" );
    if(!tmp) { err = RVI_ERR_JSON; goto exit; }

    ctx->keyfile = strdup( json_string_value( 
                json_object_get( tmp, "key" ) ) );
    ctx->certfile = strdup( json_string_value( 
                json_object_get( tmp, "cert" ) ) );
    ctx->id = strdup( json_string_value(
                json_object_get ( tmp, "id" ) ) );

    tmp = json_object_get ( conf, "ca" );
    if(!tmp) { err = RVI_ERR_JSON; goto exit; }

    ctx->cadir = strdup( json_string_value( 
                json_object_get( tmp, "dir" ) ) );
    ctx->cafile = strdup( json_string_value( 
                json_object_get( tmp, "cert" ) ) );

    const char *creddir = json_string_value(
                json_object_get ( conf, "creddir" ) );
    
    if( creddir[ strlen( creddir ) - 1 ] == '/' ) {
        /* If the final character of the directory is a forward slash */
        ctx->creddir = strdup( creddir );
    } else {
        /* Otherwise, add a trailing slash */
        ctx->creddir = malloc( strlen( creddir ) + 2 );
        if(! ctx->creddir ) { err = ENOMEM; goto exit; }
        sprintf( ctx->creddir, "%s/", creddir );
    }

    if( !(ctx->creddir) ) { err = RVI_ERR_NOCRED; goto exit; }

    d = opendir( ctx->creddir );
    if (!d) { err = RVI_ERR_NOCRED; goto exit; }

    certbio = BIO_new_file( ctx->certfile, "r" );
    if( !certbio ) { err = RVI_ERR_NOCRED; goto exit; }
    cert = PEM_read_bio_X509( certbio, NULL, 0, NULL);
    if( !cert ) { err = RVI_ERR_NOCRED; goto exit; }

    int i = 0;
    char *path = NULL;
    size_t pathSize;
    while ( ( dir = readdir( d ) ) ) {
        if ( strstr( dir->d_name, ".jwt" ) ) {
            /* if it's a jwt file, open it */
            pathSize = strlen(ctx->creddir) + strlen(dir->d_name) + 1;
            path = malloc(pathSize);
            if(!path) { err = ENOMEM; goto exit; }
            sprintf(path, "%s%s", ctx->creddir, dir->d_name );
            fp = fopen( path, "r" );
            if( !fp ) { err = RVI_ERR_NOCRED; goto exit; }
            /* go to end of file */
            if( fseek( fp, 0L, SEEK_END ) != 0 ) { err = RVI_ERR_NOCRED; goto exit; }
            /* get value of file position indicator */
            long bufsize = ftell(fp);
            if( bufsize == -1 ) { err = RVI_ERR_NOCRED; goto exit; }
            cred = malloc( bufsize + 1 );
            if( !cred ) { err = ENOMEM; goto exit; }
            /* go back to start of file */
            rewind( fp );
            /* read the entire file into memory */
            size_t len = fread( cred, sizeof(char), bufsize, fp );
            if( ferror( fp ) != 0) {
                fputs("Error reading credential file", stderr);
            } else {
                cred[len] = '\0'; /* Ensure string is null-terminated */
            }
            if( rviValidateCredential( handle, cred, cert ) == RVI_OK ) {
                rviListInsert( ctx->creds, cred );
            }
            fclose( fp );
            free(path);
            i++;
        }
    }

exit:
    BIO_free_all( certbio );
    X509_free( cert );
    if( d ) closedir( d );

    return err;
}

/* This utility function returns a pointer to the first delim in a buffer, or
 * NULL if delim does not appear in the buffer. IT DOES NOT COPY THE BUFFER: if
 * needed, the partial buffer must be copied to a new buffer and the original
 * buffer freed by the calling function.
 */
char *start_buf_at_char(char *buf, char delim)
{
    if (!buf) return NULL;
    while (*buf != '\0' && *buf != delim && *buf++);
    if (*buf != delim) {
        buf = NULL;
    }
    return buf;
}

/* This utility function destructively reads from 'src' to populate '*json'
 * with a parsed json_t object. The remaining string (possibly 'src' exactly)
 * is copied into the buf member of "remote" (and updates the buflen). The
 * original 'src' must not be accessed again by the calling function. */
int rviReadJsonChunk( json_t **json, char *src, TRviRemote *remote )
{
    json_error_t error;
    int ret = 0;
    int len;

    *json = json_loads(src, JSON_DISABLE_EOF_CHECK, &error);

    char *tmp = {0};

    if (!*json) { 
        /* If we did not get JSON, make sure the buffer starts on a valid char
         * */ 
        tmp = start_buf_at_char(src, '{');
    } else { 
        /* If we did get JSON, check the remaining buffer for additional JSON
         * strings */
        tmp = start_buf_at_char(&src[error.position], '{');
    }

    free(remote->buf);

    if (!tmp) {
        remote->buf = NULL;
        remote->buflen = 0;
        ret = RVI_ERR_JSON;
    } else {
        len = strlen(tmp);
        remote->buf = malloc(len + 1);
        if (!(remote->buf)) { ret = -ENOMEM; goto exit; }
        memset(remote->buf, 0, len + 1);
        memcpy(remote->buf, tmp, len);
        remote->buflen = len;
        ret = RVI_ERR_JSON_PART;
    }

    free(src);

exit:
    return ret;
}

/** This utility function returns a pointer to a new json_t populated with the
 * contents of the value associated with the key "grant" */
json_t *rviGetJsonGrant ( jwt_t *jwt, const char *grant )
{
    if( !jwt || !grant || !strlen(grant) ) { return NULL; }

    /* get token in plaintext */
    char *plaintext = jwt_dump_str( jwt, 0 );

    /* store pointer to free memory */
    char *start = plaintext;

    /* advance past header */
    while( *plaintext++ != '.');

    json_t *body = json_loads( plaintext, JSON_REJECT_DUPLICATES, NULL );

    free( start );

    json_t *claim = json_object_get( body, grant );

    json_t *new = json_deep_copy( claim );

    json_decref( body );

    return new;
}

/** Get arrays of rightToReceive and rightToInvoke */
int rviGetRightsFromCredential( TRviHandle handle, const char *cred, 
                           TRviList *rights )
{
    if( !handle || !cred ||  !rights ) { return EINVAL; }

    TRviContext     *ctx = (TRviContext *)handle;
    jwt_t           *jwt;
    char            *key;
    int             ret;
    time_t          rawtime;

    key = rviGetPubkeyFile( ctx->cafile );
    if( !key  ){ ret = -1; goto exit; }

    /* Load the JWT into memory from base64-encoded string */
    ret = jwt_decode(&jwt, cred, (unsigned char *)key, strlen(key));
    if( ret != 0 ) { goto exit; }

    /* Check that we are using public/private key cryptography */
    if( jwt_get_alg( jwt ) != JWT_ALG_RS256 ) { ret = 1; goto exit; }
    
    /* Check validity: start/stop */
    time(&rawtime);
    json_t *validity = rviGetJsonGrant( jwt, "validity" );

    int start = json_integer_value( json_object_get( validity, "start" ) );
    int stop = json_integer_value( json_object_get( validity, "stop" ) );

    if( ( start > rawtime ) || ( stop < rawtime ) ) { ret = -1; goto exit; }
    
    /* Load the rights to receive */
    json_t *receive = rviGetJsonGrant( jwt, "right_to_receive" );
    char *rcv = json_dumps(receive, JSON_ENCODE_ANY);
    json_decref(receive);
    /* Load the right to invoke */
    json_t *invoke = rviGetJsonGrant(jwt, "right_to_invoke" );
    char *inv = json_dumps(invoke, JSON_ENCODE_ANY);
    json_decref(invoke);

    TRviRights *new = rviRightsCreate( rcv, inv, stop );

    rviListInsert( rights, new );
 
    free( rcv );

    free( inv );

exit:
    free(key);
    jwt_free(jwt);
    if ( validity ) json_decref( validity );

    return ret;
}

int rviRightToReceiveError( TRviList *rlist, const char *serviceName )
{
    if( !rlist || !serviceName ) { return EINVAL; }

    int     err     = -1; /* By default, assume no rights */
    json_t  *value  = NULL;
    size_t  index;

    TRviListEntry *ptr = rlist->listHead;
    while( ptr ) {
        TRviRights *tmp = (TRviRights *)ptr->pointer;
        if( !tmp ) goto exit;
        //json_array_foreach( tmp->receive, index, value ) {
        for( index = 0; 
             index < json_array_size( tmp->receive ) && ( value = json_array_get( tmp->receive, index ) ); 
             index ++) {
            const char *pattern = json_string_value( value );
            if( ( err = rviComparePattern( pattern, serviceName ) ) == RVI_OK )
                goto exit; /* We found a match, exit immediately */
        }
        ptr = ptr->next;
    }

exit:
    return err;
}

int rviRightToInvokeError( TRviList *rlist, const char *serviceName )
{
    if( !rlist || !serviceName ) { return EINVAL; }

    int     err     = -1; /* By default, assume no rights */
    json_t  *value  = NULL;
    size_t  index;

    TRviListEntry *ptr = rlist->listHead;
    while( ptr ) {
        TRviRights *tmp = (TRviRights *)ptr->pointer;
        //json_array_foreach( tmp->invoke, index, value ) {
        for( index = 0; 
             index < json_array_size( tmp->invoke ) && ( value = json_array_get( tmp->invoke, index ) ); 
             index ++) {
            const char *pattern = json_string_value( value );
            if( ( err = rviComparePattern( pattern, serviceName ) ) == RVI_OK )
                goto exit; /* We found a match, exit immediately */
        }
        ptr = ptr->next;
    }

exit:
    return err;

}

/** Get the public key from a certificate file */
char *rviGetPubkeyFile( char *filename )
{
    if( !filename ) { return NULL; }

    EVP_PKEY    *pkey       = NULL;
    BIO         *certbio    = NULL;
    BIO         *mbio       = NULL;
    X509        *cert       = NULL;
    char        *key = 0;
    long        length;
    int         ret = RVI_OK;
    int         ok = 0; /* Status for OpenSSL calls */

    /* Get public key from root certificate */
    /* First, load PEM string into memory */
    certbio = BIO_new_file( filename, "r" );
    if( !certbio ) { ret = ENOMEM; goto exit; }
    /* Then read the certificate from the string */
    cert = PEM_read_bio_X509( certbio, NULL, 0, NULL );
    if( !cert ) { ret = 1; goto exit; } 
    /* Get the public key from the certificate */
    pkey = X509_get_pubkey(cert); 
    if( !pkey ) { ret = 1; goto exit; }
    /* Make a new memory BIO */
    mbio = BIO_new( BIO_s_mem() ); 
    if( !mbio ) { ret = ENOMEM; goto exit; }
    /* Write the pubkey to the new BIO as a PEM-formatted string */
    ok = PEM_write_bio_PUBKEY(mbio, pkey);
    if( !ok ) { ret = RVI_ERR_OPENSSL; goto exit; }

    /* Find out how long our new string is */
    length = BIO_ctrl_pending(mbio);
    /* Allocate a buffer for the key string... */
    key = malloc( length + 1 );
    if( !key ) { ret = ENOMEM; goto exit; }
    /* Load the string into memory */
    ret = BIO_read(mbio, key, length);
    if(verbose){
        fprintf(stderr, "rviGetPubkeyFile, received %d bytes, : '%s'\n", ret, key);
    }
    if( ret != length) { goto exit; }
    /* Make sure it's null-formatted, just in case */
    key[length] = '\0';

    ret = RVI_OK;

exit:
    /* Free all the memory */
    EVP_PKEY_free(pkey);
    X509_free(cert);
    BIO_free_all(certbio);
    BIO_free_all(mbio);

    if( ret != RVI_OK ) { return NULL; }
    return key;
}

/** 
 * This function tests whether a credential is valid.
 *
 * Tests:
 *  * Signed by trusted authority
 *  * Timestamp is valid
 *  * Embedded device cert matches supplied cert
 *
 * @param[in] handle    - handle to the RVI context
 * @param[in] cred      - JWT-encoded RVI credential
 * @param[in] cert      - the expected certificate for the device, e.g., peer certificate
 *
 * @return RVI_OK (0) on success, or an error code on failure.
 */
int rviValidateCredential( TRviHandle handle, const char *cred, X509 *cert )
{
    if( !handle || !cred ) { return EINVAL; }

    int             ret;
    TRviContext     *ctx = (TRviContext *)handle;
    char            *key;
    jwt_t           *jwt;
    long            length;
    time_t          rawtime;
    BIO             *bio = {0};
    X509            *dcert = {0};
    const char      certHead[] = "-----BEGIN CERTIFICATE-----\n";
    const char      certFoot[] = "\n-----END CERTIFICATE-----";
    json_t          *validity = NULL;
    char            *tmp = NULL;

    ret = RVI_OK;

    /* Get the public key from the trusted CA */
    key = rviGetPubkeyFile( ctx->cafile );

    if( !key ) { ret = -1; goto exit; }

    length = strlen(key) + 1;

    /* If token does not pass sig check, libjwt supplies errno */
    ret = jwt_decode( &jwt, cred, (unsigned char *)key, length );
    if( ret ) {
        goto exit;
    }

    /* RVI credentials use RS256 */
    if( jwt_get_alg( jwt ) != JWT_ALG_RS256 ) { ret = 1; goto exit; }

    /* Check validity: start/stop */
    time(&rawtime);
    validity = rviGetJsonGrant( jwt, "validity" );

    int start = json_integer_value( json_object_get( validity, "start" ) );
    int stop = json_integer_value( json_object_get( validity, "stop" ) );

    if( ( start > rawtime ) || ( stop < rawtime ) ) { ret = -1; goto exit; }

    const char *deviceCert = jwt_get_grant( jwt, "device_cert" );
    tmp = malloc( strlen( deviceCert ) + strlen( certHead ) 
                        + strlen ( certFoot ) + 1 );
    if( !tmp ) { ret = ENOMEM; goto exit; }
    sprintf(tmp, "%s%s%s", certHead, deviceCert, certFoot);

    /* Check that certificate in credential matches expected cert */
    bio = BIO_new( BIO_s_mem() );

    if(verbose){
        fprintf(stderr, "rviValidateCredential, sending: '%s'\n", tmp);
    }
    BIO_puts( bio, (const char *)tmp );
    dcert = PEM_read_bio_X509( bio, NULL, 0, NULL );
    if( !dcert ) { ret = RVI_ERR_OPENSSL; goto exit; }
    ret = X509_cmp( dcert, cert );

exit:
    jwt_free( jwt );
    if( key ) free( key );
    if( validity ) json_decref( validity );
    if( tmp ) free( tmp );
    BIO_free_all( bio );
    X509_free( dcert );

    return ret;
}

/*
 * Enables or disables the verbose loging, by default it is disabled.
 */

void rviSetVerboseLogs (bool verboseEnable )
{
    verbose = verboseEnable;
}


/*
 * Initialize the RVI library. Call before using any other functions.
 */


TRviHandle rviInitInternal (json_t *configContent )
{
    if( !configContent ) { return NULL; }
    /* initialize OpenSSL */
    SSL_library_init();
    SSL_load_error_strings();
    
    /* Allocate memory for an RVI context structure. 
     * This structure contains:
     *      lookup trees for services and remote connections
     *      shared SSL context factory object for generating new SSL sessions
     *      this node's permissions in the RVI architecture
     */
    TRviContext *ctx = malloc(sizeof(TRviContext));
    if(!ctx) {
        fprintf(stderr, "Unable to allocate memory\n");
        return NULL;
    }
    ctx = memset ( ctx, 0, sizeof ( TRviContext ) );

    /* Allocate a block of memory for storing credentials, then initialize each 
     * pointer to null */
    ctx->creds = malloc( sizeof( TRviList ) );
    ctx->rights = malloc( sizeof( TRviList ) );

    if( !ctx->creds || !ctx->rights ) {
        fprintf(stderr, "Unable to allocate memory\n");
        return NULL;
    }

    rviListInitialize( ctx->creds );
    rviListInitialize( ctx->rights );

    if ( rviParseJsonConfig ( ctx, configContent ) != 0 ) {
        fprintf(stderr, "Error reading config file\n");
        goto err;
    }

    if ( !(ctx->creds->count) ) {
        fprintf(stderr, "Error: no credentials available\n");
        goto err;
    }

    TRviListEntry *ptr = ctx->creds->listHead;
    while( ptr ) {
        int ret = rviGetRightsFromCredential( ctx, (char *)ptr->pointer, 
                                         ctx->rights );
        if( ret != RVI_OK ) { goto err; }
        ptr = ptr->next;
    }

    if ( !(ctx->rights->count) ) {
        fprintf(stderr, "Error: no rights available\n");
        goto err;
    }

    /* Create generic SSL context configured for client access */
    ctx->sslCtx = rviSetupClientCtx(ctx);
    if(!ctx->sslCtx) {
        fprintf(stderr, "Error setting up SSL context\n");
        goto err;
    }

    /*  
     * Create empty btrees for indexing remote connections and services. 
     * 
     * Since we expect that records will frequently be added and removed, use a 
     * small order for each tree. This means that the tree will be deeper, but 
     * addition/deletion will usually result in simply changing pointers rather 
     * than copying data. 
     */
    
    /*   
     * Remote connections will be indexed by the socket's file descriptor.    
     */  
    ctx->remoteIdx = btree_create(2, rviCompareFd);

    /*   
     * Services will be indexed by the fully-qualified service name, which is
     * unique across the RVI infrastructure. 
     */  
    ctx->serviceNameIdx = btree_create(2, rviCompareName);

    /*
     * Services will also be indexed by the file descriptor of the entity 
     * registering the service. Service names are used as a tie-breaker to 
     * ensure each record has a unique position in the tree. 
     */
    ctx->serviceRegIdx = btree_create(2, rviCompareRegistrant);
    
    return (TRviHandle)ctx;

err:
    rviCleanup(ctx);

    return NULL;
}


/*
 * Initialize the RVI library with config. Call before using any other functions.
 */

TRviHandle rviJsonInit (char *configContent )
{
    TRviHandle handle = NULL;
    json_error_t errjson;
    json_t *conf = NULL;

    conf = json_loads( configContent, 0, &errjson );
    if (!conf){
        return handle;
    }
    handle = rviInitInternal(conf);
    json_decref(conf);
    return handle;
}


/*
 * Initialize the RVI library with config file. Call before using any other functions.
 */

TRviHandle rviInit (char *configFilename)
{
    TRviHandle handle = NULL;
    json_error_t errjson;
    json_t *conf = NULL;

    conf = json_load_file( configFilename, 0, &errjson );
    if (!conf){
        return handle;
    }
    handle = rviInitInternal(conf);
    json_decref(conf);
    return handle;
}


/* 
 * Tear down the API.
 */

int rviCleanup(TRviHandle handle)
{
    if( !handle ) { return EINVAL; }

    TRviContext * ctx = (TRviContext *)handle;
    TRviRemote *  rtmp;
    TRviService * stmp;

    /* free all SSL structs */
    SSL_CTX_free(ctx->sslCtx);

    /*  
     * Destroy each tree, including all structs pointed to 
     */
    
    /*  
     * As long as the context contains remote connections, find the first 
     * struct in the tree, and disconnect the corresponding file descriptor. 
     * The disconnect function removes the entry from the tree and frees the 
     * underlying memory. 
     */

    if(ctx->remoteIdx) {
        while(ctx->remoteIdx->count != 0) {
            rtmp = (TRviRemote *)ctx->remoteIdx->root->dataRecords[0];
            if(!rtmp) {
                perror("Getting remote data in cleanup"); 
                break;
            }
            /* Disconnect the remote SSL connection, delete the entry from the 
            * tree & free the remote struct */
            rviDisconnect(handle, rtmp->fd);
        }
        btree_destroy(ctx->remoteIdx);
    }

    /* 
     * As long as the context contains services, find the first struct from 
     * either service tree. Delete the entry from each service tree, then free 
     * the underlying memory. 
     */
    if(ctx->serviceNameIdx) {
        while(ctx->serviceNameIdx->count != 0) {
            /* Delete the first data record in the root node */
            stmp = (TRviService *)ctx->serviceNameIdx->root->dataRecords[0];
            if ( !stmp ) {
                perror("Getting service data in cleanup"); 
                break;
            }
            /* Delete the entry from the service name index */
            btree_delete ( ctx->serviceNameIdx, 
                           ctx->serviceNameIdx->root, stmp);
            /* Delete the entry from the service registrant index */
            btree_delete ( ctx->serviceRegIdx, 
                           ctx->serviceRegIdx->root, stmp);
            /* Free the service memory */
            rviServiceDestroy ( stmp );
        }

        /* Destroy both service trees */
        btree_destroy(ctx->serviceNameIdx);

        btree_destroy(ctx->serviceRegIdx);
    }

    /* Free all credentials and other entities set when parsing config */
    rviCredentialListDestroy( ctx->creds );

    if( ctx->certfile )
        free ( ctx->certfile );
    if( ctx->keyfile )
        free ( ctx->keyfile );
    if( ctx->cafile )
        free ( ctx->cafile );
    if( ctx->cadir )
        free ( ctx->cadir );
    if( ctx->creddir )
        free ( ctx->creddir );
    if( ctx->id )
        free ( ctx->id );

    rviRightsListDestroy( ctx->rights );

    /* Free the memory allocated to the TRviContext struct */
    memset( ctx, 0, sizeof( TRviContext ) );
    free(ctx);

    return RVI_OK;
}

/* ************************* */
/* RVI CONNECTION MANAGEMENT */
/* ************************* */

/* 
 * Connect to a remote node at a specified address and port. 
 */
int rviConnect(TRviHandle handle, const char *addr, const char *port)
{
    /* Ensure that we have received valid arguments */
    if( !handle || !addr || !port ) { return -EINVAL; }

    BIO             *sbio   = NULL;
    SSL             *ssl    = NULL;
    TRviRemote    *remote = NULL;
    TRviContext   *ctx    = (TRviContext *)handle;
    int ret;

    ret = RVI_OK;

    /* 
     * Spawn new SSL session from handle->ctx. BIO_new_ssl_connect spawns a new
     * chain including a SSL BIO (using ctx) and a connect BIO
     */
    sbio = BIO_new_ssl_connect(ctx->sslCtx);
    if(!sbio) {
        ret = -RVI_ERR_OPENSSL;
        goto err;
    }
    BIO_get_ssl(sbio, &ssl);
    if(!ssl) {
        ret = -RVI_ERR_OPENSSL;
        goto err;
    }

    /* 
     * When performing I/O, automatically retry all reads and complete
     * negotiations before returning. Note that all BIOs have their I/O flag
     * set to blocking by default. 
     */
    SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);

    /* 
     * Set the addr and port 
     */
    BIO_set_conn_hostname(sbio, addr);
    BIO_set_conn_port(sbio, port);

    /* check if we're already connected to that host... */
    if( ctx->remoteIdx->count ) {
        btree_iter iter = btree_iter_begin( ctx->remoteIdx );
        while( !btree_iter_at_end( iter ) ) {
            TRviRemote *rtmp = btree_iter_data( iter );
            if( 0 == strcmp( BIO_get_conn_hostname ( sbio ), 
                             BIO_get_conn_hostname( rtmp->sbio )
                           )  
              ) { /* We already have a connection to that host */
                ret = -1;
                break;
            }
            btree_iter_next( iter );
        }
        btree_iter_cleanup( iter );
    }
    if( ret != RVI_OK ) goto err;

    if(BIO_do_connect(sbio) <= 0) {
        ret = -RVI_ERR_OPENSSL;
        goto err;
    }

    if(BIO_do_handshake(sbio) <= 0) {
        ret = -RVI_ERR_OPENSSL;
        goto err;
    }

    remote = rviRemoteCreate ( sbio, SSL_get_fd ( ssl ) );

    /* Add this data structure to our lookup tree */
    btree_insert(ctx->remoteIdx, remote);
    
    rviWriteAu( handle, remote ); 
    
    /* parse incoming "au" message */
    rviProcessInput( handle, &remote->fd, 1 );

    /* create JSON array of all services */
    rviAllServiceAnnounce( handle, remote );

    /* parse incoming "sa" message */
    rviProcessInput( handle, &remote->fd, 1 );

    return remote->fd;

err:
    ERR_print_errors_fp( stderr );
    rviRemoteDestroy( remote );
    BIO_free( sbio );

    return ret;
}

/* 
 * Disconnect from a remote node with a specified file descriptor. 
 */
int rviDisconnect(TRviHandle handle, int fd)
{
    if( !handle || fd < 3 ) { return -EINVAL; }
    
    TRviContext * ctx = (TRviContext *)handle;
    TRviRemote    rkey = {0};
    TRviRemote *  rtmp;
    TRviService   skey = {0};
    TRviService * stmp;
    int             res;
    
    rkey.fd = fd;

    rtmp = btree_search(ctx->remoteIdx, &rkey);
    if(!rtmp) {
        return -ENXIO;
    }

    if( ( res = btree_delete(ctx->remoteIdx, 
                             ctx->remoteIdx->root, rtmp ) ) < 0 ) {
        return res;
    } 
    /* Search the service tree for any services registered by the remote */
    skey.registrant = fd;
    while((stmp = btree_search(ctx->serviceRegIdx, &skey))) {
        /* We have a match, so delete the service and free the node from
        * the tree */
        btree_delete(ctx->serviceNameIdx, ctx->serviceNameIdx->root, stmp);
        btree_delete(ctx->serviceRegIdx, ctx->serviceRegIdx->root, stmp);
        /* Close connection & free memory for the service structure */
        rviServiceDestroy(stmp);
    }

    rviRemoteDestroy( rtmp );

    return RVI_OK;
}

/*
 * Resume the connection on the specified file descriptor
 */
int rviResumeConnection(TRviHandle handle, int fd)
{
    if( !handle || fd < 3 ) { return -EINVAL; }
    
    TRviContext     *ctx = (TRviContext *)handle;
    TRviRemote      rkey = {0};
    TRviRemote      *rtmp;
    int             ret = RVI_OK;
    
    rkey.fd = fd;

    rtmp = btree_search(ctx->remoteIdx, &rkey);
    if(!rtmp) {
        ret = ENXIO;
        goto exit;
    }

    BIO_reset(rtmp->sbio);

    if(BIO_do_connect(rtmp->sbio) <= 0) {
        ret = -RVI_ERR_OPENSSL;
        goto exit;
    }

    if(BIO_do_handshake(rtmp->sbio) <= 0) {
        ret = -RVI_ERR_OPENSSL;
        goto exit;
    }

exit:
    return ret;
}

/* 
 * Return all file descriptors in the RVI context
 */
int rviGetConnections(TRviHandle handle, int *conn, int *connSize)
{
    if( !handle || !conn || !connSize ) { return EINVAL; }

    TRviContext *ctx = (TRviContext *)handle;

    if( ctx->remoteIdx->count == 0 ) {
        *connSize = 0;
        return RVI_OK;
    }
    btree_iter iter = btree_iter_begin( ctx->remoteIdx );
    int i = 0;
    while( ! btree_iter_at_end( iter ) ) {
        if( i == *connSize )
            break;
        TRviRemote *remote = btree_iter_data( iter );
        if( ! remote )
            break;
        *conn++ = remote->fd ;
        i++;
        btree_iter_next( iter );
    }
    *connSize = i;
    btree_iter_cleanup( iter );

    return RVI_OK;
}


/* ********************** */
/* RVI SERVICE MANAGEMENT */
/* ********************** */


/* 
 * Register a service with a callback function
 */
int rviRegisterService( TRviHandle handle, const char *serviceName, 
                          TRviCallback callback, 
                          void *serviceData )
{
    if( !handle || !serviceName ) { return EINVAL; }

    int             err         = 0;
    TRviContext     *ctx        =   (TRviContext *)handle;
    TRviService     *service    = NULL;
    char            *fqsn       = NULL;

    fqsn = rviFqsnGet( handle, serviceName );
    if( !fqsn ) { return ENOMEM; }
    
    if( (err = rviRightToReceiveError( ctx->rights, fqsn ) ) ) {
        goto exit;
    }

    /* Create a new TRviService structure */
    service = rviServiceCreate( fqsn, 0, callback, serviceData );

    /* Add service to services by name */
    btree_insert( ctx->serviceNameIdx, service );
    /* Add service to services by registrant */
    btree_insert( ctx->serviceRegIdx, service );

    rviServiceAnnounce( handle, service, 1 );

exit:
    free( fqsn );

    return err;
}

/* 
 * Unregister a previously registered service
 */
int rviUnregisterService(TRviHandle handle, const char *serviceName)
{
    if( !handle || !serviceName ) { return EINVAL; }

    int             err     = RVI_OK;
    TRviContext     *ctx    = (TRviContext *)handle;
    TRviService     skey    = { 0 };
    
    skey.name = rviFqsnGet( handle, serviceName );
    TRviService *stmp = btree_search( ctx->serviceNameIdx, &skey );
    
    if( !stmp ) {
        err = -ENXIO;
        goto exit;
    }

    if( stmp->registrant != 0 ) {
        err = -RVI_ERR_RIGHTS;
        goto exit;
    }

    rviServiceAnnounce( handle, stmp, 0 );

    err = rviRemoveService( handle, skey.name );

exit:
    free( skey.name );

    return err;
}

int rviRemoveService(TRviHandle handle, const char *serviceName)
{
    if( !handle || !serviceName ) { return EINVAL; }

    TRviContext   *ctx    = (TRviContext *)handle;
    TRviService   skey    = { 0 };
    
    skey.name = strdup( serviceName );
    TRviService *stmp = btree_search( ctx->serviceNameIdx, &skey );
    free( skey.name );
    
    if( !stmp ) { return ENOENT; }
    btree_delete( ctx->serviceNameIdx, 
                          ctx->serviceNameIdx->root, stmp );
    btree_delete( ctx->serviceRegIdx, 
                          ctx->serviceRegIdx->root, stmp );
    rviServiceDestroy( stmp );

    return RVI_OK;
}

/* 
 * Get list of services available
 */
int rviGetServices(TRviHandle handle, char **result, int *len)
{
    if( !handle || !result || ( *len < 1 ) ) { return EINVAL; }

    TRviContext *ctx = (TRviContext *)handle;

    if( ctx->serviceNameIdx->count == 0 ) {
        *len = 0;
        return RVI_OK;
    }

    btree_iter iter = btree_iter_begin( ctx->serviceNameIdx );
    int i = 0;
    while( ! btree_iter_at_end( iter ) ) {
        if( i == *len )
            break;
        TRviService *service = btree_iter_data( iter );
        if( ! service )
            break;
        *result++ = strdup( service->name );
        i++;
        btree_iter_next( iter );
    }
    *len = i;
    btree_iter_cleanup( iter );

    return RVI_OK;
}

/* 
 * Invoke a remote service
 */
int rviInvokeService(TRviHandle handle, const char *serviceName, 
                              const char *parameters)
{
    if( !handle || !serviceName ) { return EINVAL; }
    /* get service from service name index */

    TRviContext *ctx = (TRviContext *)handle;
    TRviService skey = {0};
    TRviService *stmp = NULL;
    TRviRemote rkey = {0};
    TRviRemote *rtmp = NULL;
    time_t rawtime; /* the unix epoch time for the current time */
    int wait = 1000; /* the timeout length in ms */
    long long timeout;
    json_t *params = NULL;
    json_t *rcv;
    int ret;
    
    skey.name = strdup(serviceName);

    stmp = btree_search(ctx->serviceNameIdx, &skey);
    if( !stmp ) { ret = ENOENT; goto exit; }

    /* identify registrant, get SSL session from remote index */
    rkey.fd = stmp->registrant;

    rtmp = btree_search(ctx->remoteIdx, &rkey);
    if( !rtmp ) { ret = ENXIO; goto exit; }

    time(&rawtime);
    timeout = rawtime + wait;

    /* prepare rcv message */
    if( parameters ) {
        params = json_loads( parameters, 0, NULL );
    }
    if ( !params ) { ret = RVI_ERR_JSON; goto exit; }

    rcv = json_pack( 
            "{s:s, s:i, s:s, s:{s:s, s:I, s:o}}",
            "cmd", "rcv",
            "tid", 1, /* TODO: talk to Ulf about tid */
            "mod", "proto_json_rpc",
            "data", "service", serviceName,
                    "timeout", timeout,
                    "parameters", params
            );
    if( ! rcv ) {
        fprintf(stderr, "JSON error\n");
        ret = RVI_ERR_JSON;
        goto exit;
    }

    char *rcvString = json_dumps(rcv, JSON_COMPACT);

    /* send rcv message to registrant */

    if(verbose){
        fprintf(stderr, "rviInvokeService, sending: '%s'\n", rcvString);
    }

    ret = BIO_puts(rtmp->sbio, rcvString);
    if (ret < 0) {
        /* The connection was likely closed by the peer, attempt to resume */
        rviResumeConnection(handle, rtmp->fd);
    }

    free(rcvString);
    json_decref(rcv);

    ret = 0;

exit:
    free(skey.name);

    return ret;
}

/* ************** */
/* I/O MANAGEMENT */
/* ************** */

int rviProcessInput(TRviHandle handle, int *fdArr, int fdLen)
{
    if( !handle || !fdArr || ( fdLen < 1 ) ) { return EINVAL; }

    TRviContext   *ctx    = (TRviContext *)handle;
    TRviRemote    rkey    = {0};
    TRviRemote    *rtmp   = NULL;
    char            cmd[5]  = {0};

    SSL             *ssl    = NULL;
    json_t          *root   = NULL;

    int             len     = 0;
    int             read    = 0;
    char            *buf    = {0};
    long            mode    = 0;
    int             i       = 0;
    int             err     = 0;


    /* For each file descriptor we've received */
    while( i < fdLen ) {
        rkey.fd = fdArr[i]; /* Set the key to the requested fd */
        i++;
        rtmp = btree_search( ctx->remoteIdx, &rkey ); /* Find the connection */
        if( !rtmp ) {
            err = ENXIO;
            fprintf( stderr, "No connection on %d\n", rkey.fd );
            continue;
        }
        BIO_get_ssl(rtmp->sbio, &ssl);
        if( !ssl ) {
            err = RVI_ERR_OPENSSL;
            fprintf( stderr, "Error reading on fd %d, try again\n", rtmp->fd );
            continue;
        }
        /* Grab the current mode flags from the session */
        mode = SSL_get_mode ( ssl );
        /* Ensure our mode is blocking */
        SSL_set_mode( ssl, SSL_MODE_AUTO_RETRY );

        len = rtmp->buflen;
#ifdef RVI_MAX_MSG_SIZE
        if ((TLS_BUFSIZE + len) > RVI_MAX_MSG_SIZE) {
            /* Exceeded maximum message size */
            memset(rtmp->buf, 0, rtmp->buflen);
            free(rtmp->buf);
            rtmp->buf = NULL;
            rtmp->buflen = 0;
            len = 0;
            err = RVI_ERR_JSON; continue;
        }
#endif
        buf = malloc(TLS_BUFSIZE + len + 1);
        if(!buf) { err = ENOMEM; goto exit; }
        memset(buf, 0, TLS_BUFSIZE + len + 1);
        if(len) {
            memcpy(buf, rtmp->buf, len);
            memset(rtmp->buf, 0, len);
            free(rtmp->buf);
            rtmp->buflen = 0; 
            rtmp->buf = NULL;
        }

        read = SSL_read(ssl, &buf[len], TLS_BUFSIZE);

        if( read  <= 0 )  {
            err = SSL_get_error(ssl, read);
            if (err != SSL_ERROR_NONE) {
                /* The peer probably closed the connection, so reopen it */
                rviResumeConnection(handle, rkey.fd);
            }
            free(buf);
            continue;
        } 
        if(verbose){
            fprintf(stderr, "rviProcessInput, received %d bytes, : '%s'\n", read, buf);
        }
        err = rviReadJsonChunk(&root, buf, rtmp); // rviReadJsonChunk returns
                                                  // non-zero if there's data
                                                  // remaining: not strictly an
                                                  // error condition

        if (!root) { continue; }

        err = 0; // if we got valid JSON, it's not an error condition

        /* Get RVI cmd from string */
        strncpy( cmd, json_string_value( json_object_get( root, "cmd" ) ), 5 );
        /* Ensure null-termination */
        cmd[4] = 0;

        if( strcmp( cmd, "au" ) == 0 ) {
            rviReadAu( handle, root, rtmp );
        } else if( strcmp( cmd, "sa" ) == 0 ) {
            rviReadSa( handle, root, rtmp );
        } else if( strcmp( cmd, "rcv" ) == 0 ) {
            rviReadRcv( handle, root, rtmp );
        } else if( strcmp( cmd, "ping" ) == 0 ) {
            /* Echo the ping back */

            if(verbose){
                fprintf(stderr, "rviProcessInput[ping], sending: '%s'\n", buf);
            }

            len = SSL_write( ssl, "{\"cmd\":\"ping\"}", read );
            if (len != read) {
                err = SSL_get_error(ssl, read);
                if (err != SSL_ERROR_NONE) {
                    /* The peer probably closed the connection, so reopen it */
                    rviResumeConnection(handle, rkey.fd);
                }
            }

        } else { /* UNKNOWN RVI COMMAND */
            err = -RVI_ERR_NOCMD; 
        }

        /* Set the mode back to its original bitmask */
        SSL_set_mode( ssl, mode );

        json_decref( root );
    }

exit:
    return err;
}

int rviReadAu( TRviHandle handle, json_t *msg, TRviRemote *remote )
{
    if( !handle || !msg || !remote ) { return EINVAL; }

    int             err     = 0;
    SSL             *ssl    = NULL;
    size_t          index;
    json_t          *value  = NULL;
    X509            *cert   = NULL;
    json_t          *tmp    = NULL;

    tmp = json_object_get( msg, "creds" );
    if( !tmp ) {
        err = RVI_ERR_JSON;
        goto exit;
    }

    BIO_get_ssl(remote->sbio, &ssl);
    if( !ssl ) {
        err = RVI_ERR_OPENSSL;
        goto exit;
    }

    if( ! ( cert = SSL_get_peer_certificate( ssl ) ) ) {
        err = RVI_ERR_OPENSSL;
        goto exit;
    }

//    json_array_foreach( tmp, index, value ) {
    for( index = 0; 
         index < json_array_size( tmp ) && ( value = json_array_get( tmp, index ) ); 
         index ++) {
        const char *val = json_string_value( value );
        if(  rviValidateCredential( handle, val, cert ) != RVI_OK ) {
            continue;
        }
        err = rviGetRightsFromCredential( handle, val, remote->rights );
        if( err ) goto exit;
    }

exit:
    if( cert ) X509_free( cert );
    return err;
}

int rviWriteAu( TRviHandle handle, TRviRemote *remote )
{
    if( !handle || !remote ) { return EINVAL; }

    int             err     = RVI_OK;
    TRviContext     *ctx    = ( TRviContext * )handle;
    json_t          *creds  = NULL;
    json_t          *au     = NULL;
    char            *auString = NULL;

    creds = json_array();
    TRviListEntry *ptr = ctx->creds->listHead;
    while( ptr ) {
        json_array_append_new( creds, json_string( (char *)ptr->pointer ) );
        ptr = ptr->next;
    }

#ifdef RVI_MAX_MSG_SIZE
    au = json_pack( "{s:s, s:s, s:o, s:I}", 
                    "cmd", "au",            /* populate cmd */
                    "ver", "1.1",           /* populate version */
                    "creds", creds,          /* fill with json array */
                    "max_msg_size", RVI_MAX_MSG_SIZE /* include max msg size */
                  );
#else
    au = json_pack( "{s:s, s:s, s:o}", 
                    "cmd", "au",            /* populate cmd */
                    "ver", "1.1",           /* populate version */
                    "creds", creds   /* fill with json array */
                  );
#endif /* RVI_MAX_MSG_SIZE */
    if( !au ) {
        err = RVI_ERR_JSON;
        goto exit;
    }

    auString = json_dumps(au, JSON_COMPACT);

    /* send "au" message */

    if(verbose){
        fprintf(stderr, "rviWriteAu, sending: '%s'\n", auString);
    }
  
    err = BIO_puts( remote->sbio, auString );
    if (err < 0) {
        /* The connection was likely closed by the peer, attempt to resume */
        rviResumeConnection(handle, remote->fd);
    }


exit:
    free( auString );
    json_decref( au );

    return err;
}

int rviReadSa( TRviHandle handle, json_t *msg, TRviRemote *remote )
{
    if( !handle || !msg || !remote ) { return EINVAL; }

    int             err     = 0;
    TRviContext   *ctx    = ( TRviContext * )handle;
    size_t          index;
    json_t          *value  = NULL;
    json_t          *tmp    = NULL;
    int             av      = 0;

    tmp = json_object_get( msg, "svcs" );
    if( !tmp ) {
        err = RVI_ERR_JSON;
        goto exit;
    }

    const char *stat = json_string_value( json_object_get( msg, "stat" ) );
    if( strcmp( stat, "av" ) == 0 ) {
        av = 1;
    }

    //json_array_foreach( tmp, index, value ) {
    for( index = 0; 
         index < json_array_size( tmp ) && ( value = json_array_get( tmp, index ) ); 
         index ++) {
        const char *val = json_string_value( value );
        if( av ) { /* Service newly available */
            /* If remote doesn't have right to receive, discard */
            if ( ( err = rviRightToReceiveError( remote->rights, val ) ) ) 
                continue;
            /* If we don't have right to invoke, discard */
            if ( ( err = rviRightToInvokeError( ctx->rights, val ) ) )
                continue;
            
            /* Otherwise, add the service to services available */
            TRviService *service = rviServiceCreate( 
                                                 val, remote->fd, 
                                                 NULL, NULL 
                                                    );
            btree_insert( ctx->serviceNameIdx, service );
            btree_insert( ctx->serviceRegIdx, service );
        } else { /* Service not available, find it and remove it */
            /* If remote doesn't have right to receive, ignore this message */
            if ( ( err = rviRightToReceiveError( remote->rights, val ) ) ) 
                continue;
            rviRemoveService( handle, val );
        }
    }

exit:
    return err;
}

int rviAllServiceAnnounce( TRviHandle handle, TRviRemote *remote )
{
    if( !handle || !remote ) { return EINVAL; }


    int             err     = RVI_OK;
    TRviContext     *ctx    = ( TRviContext * )handle;
    json_t          *svcs   = NULL;
    json_t          *sa     = NULL;
    char            *saString = NULL;

    svcs = json_array();
    if( ctx->serviceNameIdx->count ) {
        btree_iter iter = btree_iter_begin( ctx->serviceNameIdx );
        while ( !btree_iter_at_end( iter ) ) {
            TRviService *stmp = btree_iter_data( iter );
            if ( /* The remote is allowed to invoke it */
                 !( err = rviRightToInvokeError( remote->rights, stmp->name ) ) &&
                 /* The service was registered locally */
                 stmp->registrant == 0
               ) {
                json_array_append_new( svcs, json_string( stmp->name ) );
            }
            btree_iter_next( iter );
        }
        btree_iter_cleanup( iter );
    }

    sa = json_pack( "{s:s, s:s, s:o}", 
            "cmd", "sa",            /* populate cmd */
            "stat", "av",           /* populate status */
            "svcs", svcs              /* fill with array of services */
            ); 
    if( !sa ) {
        err = RVI_ERR_JSON;
        goto exit;
    }

    /* send "sa" reply */
    saString = json_dumps(sa, JSON_COMPACT);


    if(verbose){
        fprintf(stderr, "rviAllServiceAnnounce, sending: '%s'\n", saString);
    }

    err = BIO_puts( remote->sbio, saString );
    if (err < 0) {
        /* The connection was likely closed by the peer, attempt to resume */
        rviResumeConnection(handle, remote->fd);
    }


exit:
    free(saString);
    json_decref(sa);

    return err;
}

int rviServiceAnnounce( TRviHandle handle, TRviService *service, int available )
{
    if( !handle || !service ) { return EINVAL; }

    int             err     = RVI_OK;
    TRviContext   *ctx    = (TRviContext *)handle;
    json_t          *svcs   = NULL;
    json_t          *sa     = NULL;
    char            *saString = NULL;

    svcs = json_pack( "[s]", service->name );
    if( ( err = rviRightToReceiveError( ctx->rights, service->name ) ) ) {
        err = -RVI_ERR_RIGHTS; 
        goto exit;
    }

    if( service->registrant != 0 ) {
        err = -RVI_ERR_RIGHTS; 
        goto exit;
    }

    if( !(ctx->remoteIdx->count) ) {
        err = -ENXIO; 
        goto exit;
    }

    sa = json_pack( "{s:s, s:s, s:o}",
            "cmd", "sa",                        /* populate cmd */
            "stat", (available ? "av" : "un"),  /* populate status */
            "svcs", svcs                        /* fill with services */
            );

    saString = json_dumps(sa, JSON_COMPACT);

    btree_iter iter = btree_iter_begin( ctx->remoteIdx );
        while( !btree_iter_at_end( iter  ) ) {
            TRviRemote *remote = btree_iter_data( iter );
            if( ( err = rviRightToInvokeError( remote->rights, service->name ) ) ) {
                btree_iter_next( iter );
                continue; /* If the remote can't invoke, don't announce */
            }

            if(verbose){
                fprintf(stderr, "rviServiceAnnounce, sending: '%s'\n", saString);
            }

            err = BIO_puts( remote->sbio, saString );
            if (err < 0) {
                /* The connection was likely closed by the peer, resume and try again */
                rviResumeConnection(handle, remote->fd);
                continue;
            }

            btree_iter_next( iter );
        }
    btree_iter_cleanup( iter );


exit:
    if( saString ) free( saString );
    json_decref(sa);

    return err;
}

int rviReadRcv( TRviHandle handle, json_t *msg, TRviRemote *remote )
{
    if( !handle || !msg || !remote ) { return EINVAL; }

    int             err     = 0;
    TRviContext   *ctx    = ( TRviContext * )handle;
    json_t          *tmp    = NULL;
    json_t          *params = NULL;
    TRviService   skey    = {0};
    TRviService   *stmp   = NULL;
    char            *parameters = NULL;
    time_t          rawtime;
    const char      *sname;

    tmp = json_object_get( msg, "data" );
    if( !tmp ) { err = RVI_ERR_JSON; goto exit; }

    long long timeout = json_integer_value( json_object_get( tmp, "timeout" ) );
    time(&rawtime);
    if( rawtime > timeout ) { err = RVI_ERR_JSON; goto exit; }

    sname = json_string_value( json_object_get( tmp, "service" ) );
    if( ( err = rviRightToInvokeError( remote->rights, sname ) ) )
        goto exit; /* Remote does not have right to invoke */
    if( ( err = rviRightToReceiveError( ctx->rights, sname ) ) )
        goto exit; /* This node does not have the right to receive */

    skey.name = strdup( sname );
    stmp = btree_search( ctx->serviceNameIdx, &skey );
    if( !stmp ) { err = ENXIO; goto exit; }

    params = json_object_get( tmp, "parameters" );
    if( !params ) { err = RVI_ERR_JSON; goto exit; }
    parameters = json_dumps( params, JSON_COMPACT );

    stmp->callback( remote->fd, stmp->data, parameters );

exit:
    if( skey.name ) free( skey.name );
    if( parameters ) free( parameters );
    return err;
}
