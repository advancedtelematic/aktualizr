#include "ipsecondarydiscovery.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>


int IpSecondaryDiscovery::discover(){
    int discovered_ecus = 0;
    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct addrinfo hints{};
    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
    hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */
    hints.ai_protocol = 0;          /* Any protocol */
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    s = getaddrinfo(NULL, "", &hints, &result);
    
    return discovered_ecus;
}
