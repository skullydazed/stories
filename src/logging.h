#ifndef STORIES_LOGGING_H
#define STORIES_LOGGING_H

#include "config.h"

#include <pthread.h>
#include <stdio.h>

/* Owns file streams and the mutex; borrows immutable configuration until close.
 * A zero-initialized owner may always be closed. Standard streams are borrowed. */
struct logging
{
	const struct configuration *configuration;
	FILE                       *access;
	FILE                       *error;
	pthread_mutex_t             mutex;
	bool                        initialized;
};

bool logging_open(struct logging *logging, const struct configuration *configuration, char *error, size_t error_size);
/* On failure, both existing destinations remain usable and unchanged. */
bool logging_reopen(struct logging *logging, char *error, size_t error_size);
void logging_close(struct logging *logging);
void logging_access(struct logging *logging, const char *message);
/* NULL selects stderr for diagnostics before logging has been configured. */
void logging_error(struct logging *logging, const char *format, ...) __attribute__((format(printf, 2, 3)));

#endif
