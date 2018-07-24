#include <stdio.h>
#include <stdlib.h>
#include "edsign.h"

// main <public_key_in_hex> <private_key_in_hex> /path/to/message
int main(int argc, const char** argv)
{
        (void) argc;

	uint8_t signature[EDSIGN_SIGNATURE_SIZE];
	uint8_t public[EDSIGN_PUBLIC_KEY_SIZE];
	uint8_t secret[EDSIGN_SECRET_KEY_SIZE];

	// no input check is performed
	uint8_t hex_byte[3];

	for(int i = 0; i < 32; i++) {
		hex_byte[0] = argv[1][2*i];
		hex_byte[1] = argv[1][2*i + 1];
		hex_byte[2] = 0;

		public[i] = strtol((const char*)hex_byte, NULL, 16);

		hex_byte[0] = argv[2][2*i];
		hex_byte[1] = argv[2][2*i + 1];
		hex_byte[2] = 0;

		secret[i] = strtol((const char*)hex_byte, NULL, 16);
	}

	FILE* message_file = fopen(argv[3], "r");
	fseek(message_file, 0, SEEK_END);
	long size = ftell(message_file);
	rewind(message_file);

	uint8_t* message = malloc(size);

	for(unsigned long i = 0; i < size; i++) {
		message[i] = fgetc(message_file);
	}

	fclose(message_file);

	edsign_sign(signature, public, secret, message, size);

	for(int i = 0; i < EDSIGN_SIGNATURE_SIZE; i++)
		printf("%02X", signature[i]);
	printf("\n");

	return 0;
}
