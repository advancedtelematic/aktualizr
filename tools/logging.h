#ifndef SOTA_CLIENT_TOOLS_LOGGING_H_
#define SOTA_CLIENT_TOOLS_LOGGING_H_

extern int logging_verbosity;

// Use like:
// curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, get_curlopt_verbose());

long get_curlopt_verbose();

#endif
