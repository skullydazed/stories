# Deployment

Stories serves HTTP in the foreground. A reverse proxy such as nginx handles public HTTPS and caching; a service supervisor starts and stops the Stories process. Run it under an unprivileged account with read access to the content and public asset roots.

## Install from source

From the checkout:

```sh
cmake -B build-release -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local -DBUILD_TESTING=OFF
cmake --build build-release
cmake --install build-release
```

The install command needs permission to write to the selected prefix. CMake installs `bin/stories`, the project documentation, the editable page template, Pico CSS, custom CSS and theme-switcher assets, optional bundled themes, deployment examples, and licenses. With the prefix above, documentation is under `/usr/local/share/stories/docs`, the template under `/usr/local/share/stories/templates/page.html`, assets under `/usr/local/share/stories/assets`, and examples under `/usr/local/share/stories/examples`.

Installation does not create a service, overwrite `/etc/stories.conf`, create a separate site's content tree, or start the server.

## Use a release archive

Release archives contain `bin/stories`, the README, and the same `share/` tree as an installation. CivetWeb and MD4C are statically linked; the binary still requires platform system libraries.

| Target | Runtime baseline |
| --- | --- |
| `linux-x86_64` | Ubuntu 22.04 / glibc 2.35 or newer; this is a glibc build. |
| `macos-arm64` | macOS 15 on Apple Silicon. |
| `macos-x86_64` | macOS 15 on Intel. |
| `freebsd14-amd64` | FreeBSD 14.4 on amd64. |

Download the matching `stories-VERSION-TARGET.tar.gz` and its `.sha256` file from the [project releases](https://github.com/skullydazed/stories/releases). Verify the checksum and extract the archive. The macOS binaries are not Developer ID signed or notarized.

You can run the extracted `bin/stories` in place or install the tree under a chosen prefix. Relative paths in a configuration resolve against the process working directory.

## Serve the project documentation

The supplied documentation configuration uses `docs/`, `assets/`, and `templates/` relative to the process working directory. Run it from the checkout root or the installed `share/stories/` directory.

For a source checkout:

```sh
./build/stories --config examples/docs.conf.example
```

For an installation under `/usr/local`:

```sh
cd /usr/local/share/stories
stories --config examples/docs.conf.example --check-config
stories --config examples/docs.conf.example
```

For an extracted release, run these commands from the extracted directory:

```sh
cd share/stories
../../bin/stories --config examples/docs.conf.example --check-config
../../bin/stories --config examples/docs.conf.example
```

The default endpoint is `127.0.0.1:8080`. For a separate deployment layout, copy the `docs/` tree to your chosen content root, provide an asset root containing `pico.min.css`, `dark_toggle.css`, `theme.css`, `site.css`, and `dark_toggle.js`, and copy `templates/page.html` outside those public roots. Set both roots and `[site] template` explicitly in your configuration. Use paths relative to the directory from which Stories will run, or use absolute paths. Existing configurations need the required `template` setting when upgrading.

## Configure nginx

The repository's [nginx example](https://github.com/skullydazed/stories/blob/main/examples/nginx.conf) is also installed as `share/stories/examples/nginx.conf`. Include its directives in the appropriate nginx contexts and adjust the hostname, certificate paths, upstream endpoint, and cache location for your deployment. The `proxy_cache_path` directive belongs in nginx's `http` context.

Both server blocks reject `%00` by matching the original `$request_uri`, before redirecting or proxying:

```nginx
if ($request_uri ~ "%00") {
    return 400;
}
```

The match includes query parameters and leaves `%2500` untouched. CivetWeb performs URL decoding, and direct backend requests may truncate at a decoded NUL. The backend does not implement a separate encoded-NUL rejection path. [nginx documents `$request_uri`](https://nginx.org/en/docs/http/ngx_http_core_module.html#var_request_uri) as the original request URI including arguments.

Keep Stories bound to loopback when nginx runs on the same host. Make the cache directory writable by nginx, and test the configuration before reloading:

```sh
nginx -t
```

The example caches successful pages and permanent redirects for 60 seconds, leaves 404s uncached, and lets the proxy control freshness for static assets. Edits and redirect changes can remain hidden until cached responses expire. Choose the cache policy to match your publishing process; nginx documents the controls in its [proxy caching reference](https://nginx.org/en/docs/http/ngx_http_proxy_module.html#proxy_cache_valid).

After applying the example, verify `/index%00junk` and `/?value=%00` return 400, while `/assets/%70ico.min.css` still serves the stylesheet. `%2500` is not rejected by this rule; the backend resolves the resulting literal `%00` filename normally.

## Log files

Stories defaults to access records on stdout and error diagnostics on stderr. The service account needs write access to configured log files and their parent directories. Use `[logging] access_log` and `error_log`, or `--access-log` and `--error-log`, for independent append-mode destinations. See [Logging](configuration.md#logging) for path resolution and startup behavior.

After external rename/create rotation, send SIGUSR1 to the Stories process to reopen its configured log paths. A failed reopen retains both current destinations and reports the failure. Restarting also reopens the paths. Rotation and retention belong to the service's external log-management policy.

## Run as a FreeBSD service

The [rc.d example](https://github.com/skullydazed/stories/blob/main/examples/stories.freebsd) uses FreeBSD's `daemon` and assumes the binary is installed at `/usr/local/bin/stories`. Create `/etc/stories.conf` with absolute content, asset, and template paths, then install the script and enable the service:

```sh
install -m 0555 /usr/local/share/stories/examples/stories.freebsd /usr/local/etc/rc.d/stories
sysrc stories_enable=YES
sysrc stories_config=/etc/stories.conf
service stories start
```

These are system administration commands. The script defaults to user `www`; set `stories_user` in `rc.conf` when using another account. That account needs read access and directory traversal permission for both roots. Adapt the script's binary path when using another installation prefix. The [FreeBSD Handbook](https://docs.freebsd.org/en/books/handbook/config/#configtuning-rcd) describes service configuration and rc.d management.

Stop or restart through the service manager:

```sh
service stories stop
service stories restart
```

On other systems, configure the service supervisor to execute the foreground binary with `--config` pointing to the deployment configuration and to send SIGTERM when stopping it.

## Updates and shutdown

SIGINT and SIGTERM stop the listener, wait for CivetWeb workers, and then release application resources. A normal shutdown exits successfully. Configuration changes and replacement of the content-root directory require a restart. Replacing individual documents inside the existing root does not.

Use `--check-config` before restarting after configuration edits. Verify `/`, a representative document, and `/assets/pico.min.css` through the proxy after deployment. Check cache freshness when an uncached backend response differs from the public response.

[Configuration](configuration.md) · [Troubleshooting](troubleshooting.md) · [Documentation home](index.md)
