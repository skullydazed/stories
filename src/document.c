#include "document.h"
#include "url.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static bool missing_error(int error)
{
	/* O_NOFOLLOW reports EMLINK on FreeBSD and ELOOP on Linux/macOS. */
	return error == ENOENT || error == ENOTDIR || error == ELOOP || error == EMLINK || error == EACCES;
}

/* Walk each component with O_NOFOLLOW. Resolving a string with realpath and
 * opening it later would allow a rename/symlink race between those operations. */
static int open_document(int root_fd, const char *path, size_t length)
{
	char copy[STORIES_PATH_LIMIT];
	if (length >= sizeof(copy))
	{
		errno = ENAMETOOLONG;
		return -1;
	}
	memcpy(copy, path, length + 1);
	/* Set close-on-exec atomically, without a dup/fcntl window between workers. */
	int parent = fcntl(root_fd, F_DUPFD_CLOEXEC, 0);
	if (parent < 0)
		return -1;
	char *component = copy;
	for (;;)
	{
		char *slash = strchr(component, '/');
		if (slash)
			*slash = '\0';
		if (!*component || component[0] == '.' || !strcmp(component, ".."))
		{
			close(parent);
			errno = ENOENT;
			return -1;
		}
		int flags = O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK;
		if (slash)
			flags |= O_DIRECTORY;
		int fd = openat(parent, component, flags);
		int saved_errno = errno;
		close(parent);
		errno = saved_errno;
		if (fd < 0)
			return -1;
		if (!slash)
		{
			struct stat status;
			if (fstat(fd, &status) || !S_ISREG(status.st_mode))
			{
				close(fd);
				errno = ENOENT;
				return -1;
			}
			return fd;
		}
		parent = fd;
		component = slash + 1;
	}
}

static bool try_document(int root_fd, const char *source, bool redirect, struct document_route *route)
{
	size_t length = strlen(source);
	if (length >= sizeof(route->source))
		return false;
	int fd = open_document(root_fd, source, length);
	if (fd < 0)
	{
		if (!missing_error(errno) && errno != ENAMETOOLONG)
			route->status = ROUTE_FAILURE;
		return false;
	}
	route->fd = fd;
	memcpy(route->source, source, length + 1);
	if (!url_document_location(source, &route->location))
	{
		route->status = ROUTE_FAILURE;
		return true;
	}
	route->status = redirect ? ROUTE_REDIRECT : ROUTE_DOCUMENT;
	return true;
}

void document_resolve(int root_fd, const struct configuration *configuration, const char *path, size_t size, struct document_route *route)
{
	char relative[STORIES_PATH_LIMIT];
	char candidate[STORIES_PATH_LIMIT];
	*route = (struct document_route){.status = ROUTE_MISSING, .fd = -1};
	buffer_init(&route->location, STORIES_PATH_LIMIT * 3);
	if (!url_path_valid(path, size))
	{
		route->status = ROUTE_INVALID;
		return;
	}
	if (url_path_has_extension(path, size))
		return;
	memcpy(relative, path + 1, size - 1);
	relative[size - 1] = '\0';
	bool directory = path[size - 1] == '/';
	int  length = snprintf(candidate, sizeof(candidate), "%s%s", relative, directory ? "index.md" : ".md");
	if (length < 0 || length >= (int)sizeof(candidate))
		return;
	bool index = !directory && (!strcmp(relative, "index") || (strlen(relative) >= 6 && !strcmp(relative + strlen(relative) - 6, "/index")));
	if (try_document(root_fd, candidate, index, route) || route->status == ROUTE_FAILURE || directory)
		return;
	length = snprintf(candidate, sizeof(candidate), "%s/index.md", relative);
	if (length >= 0 && length < (int)sizeof(candidate) && try_document(root_fd, candidate, true, route))
		return;
	if (route->status == ROUTE_FAILURE || strchr(relative, '/'))
		return;

	/* A qualified name is authoritative. Prefer the longest matching suffix
	 * when configured directory names themselves contain underscores. */
	const char *qualified = NULL;
	size_t      qualified_length = 0;
	size_t      name_length = strlen(relative);
	for (size_t i = 0; i < configuration->directory_count; i++)
	{
		const char *name = configuration->directories[i];
		size_t      length_name = strlen(name);
		if (length_name > qualified_length && name_length > length_name + 1 && relative[name_length - length_name - 1] == '_' && !strcmp(relative + name_length - length_name, name))
		{
			qualified = name;
			qualified_length = length_name;
		}
	}
	if (qualified)
	{
		relative[name_length - qualified_length - 1] = '\0';
		length = snprintf(candidate, sizeof(candidate), "%s/%s.md", qualified, relative);
		if (length >= 0 && length < (int)sizeof(candidate))
			try_document(root_fd, candidate, true, route);
		return;
	}
	for (size_t i = 0; i < configuration->directory_count; i++)
	{
		length = snprintf(candidate, sizeof(candidate), "%s/%s.md", configuration->directories[i], relative);
		if (length >= 0 && length < (int)sizeof(candidate) && try_document(root_fd, candidate, true, route))
			return;
		if (route->status == ROUTE_FAILURE)
			return;
	}
}

void document_route_free(struct document_route *route)
{
	if (route->fd >= 0)
		close(route->fd);
	buffer_free(&route->location);
	*route = (struct document_route){.fd = -1};
}

bool document_read(int fd, size_t limit, struct buffer *output)
{
	struct stat status;
	buffer_init(output, limit);
	if (fstat(fd, &status) || status.st_size < 0 || (unsigned long long)status.st_size > limit)
		return false;
	char block[16384];
	for (;;)
	{
		ssize_t count = read(fd, block, sizeof(block));
		if (count < 0 && errno == EINTR)
			continue;
		if (count < 0)
			return false;
		if (!count)
			return buffer_append(output, "", 0);
		/* Bound the actual read too: a publisher may grow the file after fstat. */
		if (!buffer_append(output, block, (size_t)count))
			return false;
	}
}
