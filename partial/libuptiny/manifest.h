#ifndef LIBUPTINY_MANIFEST_H
#define LIBUPTINY_MANIFEST_H

#ifdef __cplusplus
extern "C" {
#endif

void uptane_write_manifest(char* signed_part, char* signatures_part);

#ifdef __cplusplus
}
#endif
#endif  // LIBUPTINY_MANIFEST_H
