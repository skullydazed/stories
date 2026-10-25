#ifndef STORIES_SERVER_H
#define STORIES_SERVER_H

#include "config.h"

/* Blocks until SIGINT/SIGTERM. Configuration is borrowed until all workers stop. */
int  server_run(const struct configuration *configuration);
bool server_check_roots(const struct configuration *configuration, char *error, size_t error_size);

#endif
