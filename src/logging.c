#include "logging.h"

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static FILE *open_log(const char *path, FILE *fallback, char *error, size_t error_size)
{
	if (!path)
		return fallback;
	/* Nonblocking open prevents a mistaken FIFO path from hanging startup. */
	int fd = open(path, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC | O_NONBLOCK, 0666);
	if (fd >= 0)
	{
		FILE *stream = fdopen(fd, "a");
		if (stream)
			return stream;
		int saved_errno = errno;
		close(fd);
		errno = saved_errno;
	}
	snprintf(error, error_size, "cannot open log %s: %s", path, strerror(errno));
	return NULL;
}

static void close_log(FILE *stream, const char *path)
{
	/* A configured path owns its stream; an omitted path borrows stdout/stderr. */
	if (stream && path && fclose(stream) == EOF)
		logging_error(NULL, "cannot close log file: %s", strerror(errno));
}

bool logging_open(struct logging *logging, const struct configuration *configuration, char *error, size_t error_size)
{
	*logging = (struct logging){.configuration = configuration};
	int result = pthread_mutex_init(&logging->mutex, NULL);
	if (result)
	{
		snprintf(error, error_size, "cannot initialize logging: %s", strerror(result));
		return false;
	}
	logging->initialized = true;
	if (!logging_reopen(logging, error, error_size))
	{
		logging_close(logging);
		return false;
	}
	return true;
}

bool logging_reopen(struct logging *logging, char *error, size_t error_size)
{
	const struct configuration *configuration = logging->configuration;
	FILE                       *access = open_log(configuration->access_log_path, stdout, error, error_size);
	if (!access)
		return false;
	FILE *errors = open_log(configuration->error_log_path, stderr, error, error_size);
	if (!errors)
	{
		close_log(access, configuration->access_log_path);
		return false;
	}
	/* Workers may finish records while replacement files are being opened, but
	 * no worker may retain an old stream after it is replaced and closed. */
	pthread_mutex_lock(&logging->mutex);
	close_log(logging->access, logging->configuration->access_log_path);
	close_log(logging->error, logging->configuration->error_log_path);
	logging->access = access;
	logging->error = errors;
	pthread_mutex_unlock(&logging->mutex);
	return true;
}

void logging_close(struct logging *logging)
{
	if (logging->initialized)
	{
		/* The owner stops all workers before releasing logging state. */
		close_log(logging->access, logging->configuration->access_log_path);
		close_log(logging->error, logging->configuration->error_log_path);
		pthread_mutex_destroy(&logging->mutex);
	}
	*logging = (struct logging){0};
}

static bool write_record(FILE *stream, const char *prefix, const char *message)
{
	bool success = true;
	flockfile(stream);
	if (fputs(prefix, stream) == EOF)
		success = false;
	/* Keep untrusted request/header text on one line, including terminal controls. */
	for (const unsigned char *p = (const unsigned char *)message; *p && success; p++)
	{
		if (*p < 32 || *p == 127)
			success = fprintf(stream, "\\x%02X", (unsigned)*p) >= 0;
		else
			success = fputc(*p, stream) != EOF;
	}
	if (success)
		success = fputc('\n', stream) != EOF;
	if (fflush(stream) == EOF)
		success = false;
	if (!success)
		clearerr(stream);
	funlockfile(stream);
	return success;
}

static void log_record(struct logging *logging, bool access, const char *prefix, const char *message)
{
	if (logging)
		pthread_mutex_lock(&logging->mutex);
	FILE *stream = logging ? (access ? logging->access : logging->error) : stderr;
	if (!write_record(stream, prefix, message) && stream != stderr)
	{
		write_record(stderr, "stories: ", access ? "cannot write access log; record follows" : "cannot write error log; record follows");
		write_record(stderr, prefix, message);
	}
	if (logging)
		pthread_mutex_unlock(&logging->mutex);
}

void logging_access(struct logging *logging, const char *message)
{
	log_record(logging, true, "", message);
}

void logging_error(struct logging *logging, const char *format, ...)
{
	int     saved_errno = errno;
	char    message[4096], prefix[64] = "stories: ";
	va_list arguments;
	va_start(arguments, format);
	vsnprintf(message, sizeof(message), format, arguments);
	va_end(arguments);
	time_t    now = time(NULL);
	struct tm utc;
	if (gmtime_r(&now, &utc))
		strftime(prefix, sizeof(prefix), "%Y-%m-%dT%H:%M:%SZ stories: ", &utc);
	log_record(logging, false, prefix, message);
	errno = saved_errno;
}
