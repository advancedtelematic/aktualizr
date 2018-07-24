#include <stdio.h>
#include <stdlib.h>
#include "edsign.h"

// main <public_key_in_hex> <private_key_in_hex> /path/to/message
int main(int argc, const char** argv)
{
        (void) argc;
        (void) argv;

	uint8_t public[EDSIGN_PUBLIC_KEY_SIZE];
	uint8_t secret[EDSIGN_SECRET_KEY_SIZE];

	FILE* rand = fopen("/dev/urandom", "r");

	for(int i = 0; i < EDSIGN_SECRET_KEY_SIZE; i++) {
		secret[i] = fgetc(rand);
		printf("%02x", secret[i]);
	}
	printf(":");

	fclose(rand);
	edsign_sec_to_pub(public, secret);

	for(int i = 0; i < EDSIGN_PUBLIC_KEY_SIZE; i++)
		printf("%02x", public[i]);
	printf("\n");
	return 0;
}
