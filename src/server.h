#ifndef STORIES_SERVER_H
#define STORIES_SERVER_H

#include "config.h"

struct logging;

/* Blocks until SIGINT/SIGTERM; SIGUSR1 reopens logs. Both arguments are borrowed
 * until all workers stop, and logging must already be initialized. */
int  server_run(const struct configuration *configuration, struct logging *logging);
bool server_check_roots(const struct configuration *configuration, char *error, size_t error_size);

#endif
