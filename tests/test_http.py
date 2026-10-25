"""Black-box HTTP and process lifecycle tests; uses only the Python standard library."""

import concurrent.futures
import contextlib
import http.client
import os
import pathlib
import re
import selectors
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import threading
import time
import unittest


BINARY = pathlib.Path(sys.argv.pop(1)).resolve()
FIXTURES = pathlib.Path(__file__).resolve().parent / 'fixtures'


class Server:
    def __init__(self, config, cwd, *overrides):
        self.errors = tempfile.TemporaryFile()
        self.process = subprocess.Popen([str(BINARY), '--config', str(config), *overrides], cwd=cwd, stdout=subprocess.PIPE, stderr=self.errors, text=True)
        self.port = None
        try:
            deadline = time.monotonic() + 10
            with selectors.DefaultSelector() as selector:
                selector.register(self.process.stdout, selectors.EVENT_READ)
                while time.monotonic() < deadline:
                    if selector.select(timeout=0.1):
                        line = self.process.stdout.readline()
                        match = re.search(r'Listening on .+ \(port (\d+)\)', line)
                        if match:
                            self.port = int(match.group(1))
                            self.request('/')
                            return
                        if not line:
                            break
            raise RuntimeError('server did not become ready')
        except Exception:
            self.stop()
            diagnostic = self.diagnostics()
            self.errors.close()
            raise RuntimeError(diagnostic) from None

    def diagnostics(self):
        self.errors.seek(0)
        return self.errors.read().decode('utf-8', errors='replace')

    def request(self, path, method='GET', headers=None):
        connection = http.client.HTTPConnection('127.0.0.1', self.port, timeout=3)
        try:
            connection.request(method, path, headers=headers or {})
            response = connection.getresponse()
            return response.status, {key.lower(): value for key, value in response.getheaders()}, response.read()
        finally:
            connection.close()

    def raw(self, method, path):
        with socket.create_connection(('127.0.0.1', self.port), timeout=3) as connection:
            connection.sendall(f'{method} {path} HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n'.encode('ascii'))
            chunks = []
            while chunk := connection.recv(65536):
                chunks.append(chunk)
        return b''.join(chunks).split(b'\r\n\r\n', 1)

    def stop(self):
        if self.process.poll() is None:
            self.process.send_signal(signal.SIGTERM)
            try:
                self.process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait()
                raise AssertionError('server did not shut down within ten seconds')
        if self.process.stdout:
            self.process.stdout.close()

    def close(self):
        try:
            self.stop()
        finally:
            self.errors.close()


class HttpTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='stories-http-')
        self.root = pathlib.Path(self.temporary.name)
        self.content = self.root / 'content'
        self.assets = self.root / 'assets'
        shutil.copytree(FIXTURES, self.content)
        self.assets.mkdir()
        (self.assets / 'sample.txt').write_text('asset bytes\n')
        (self.content / 'private.txt').write_text('must not leak')
        self.config = self.root / 'stories.conf'
        self.servers = []

    def tearDown(self):
        try:
            for server in self.servers:
                server.close()
        finally:
            self.temporary.cleanup()

    def write_config(self, markdown_limit=8388608, output_limit=33554432):
        self.config.write_text(f'[server]\nlisten=127.0.0.1:0\nthreads=4\n[content]\nroot=content\nresolution_order=disambiguation,command,function\nmarkdown_limit={markdown_limit}\noutput_limit={output_limit}\n[assets]\nroot=assets\n[site]\ntitle=HTTP Test\n[navigation]\nHome=/\n')

    def start(self, **limits):
        self.write_config(**limits)
        server = Server(self.config, self.root)
        self.servers.append(server)
        return server

    def test_redirect_chain_and_qualified_names(self):
        server = self.start()
        status, headers, body = server.request('/time?q=one%20two')
        self.assertEqual(status, 301)
        self.assertEqual(headers['location'], '/disambiguation/time?q=one%20two')
        self.assertEqual(body, b'')
        status, _, body = server.request(headers['location'])
        self.assertEqual(status, 200)
        self.assertIn(b'Which time?', body)
        self.assertEqual(server.request('/time_command')[:2][0], 301)
        self.assertEqual(server.request('/time_command')[1]['location'], '/command/time')
        self.assertEqual(server.request('/plain_function')[0], 404)
        self.assertEqual(server.request('/missing')[0], 404)

    def test_index_rules_and_source_extensions(self):
        server = self.start()
        for path in ['/', '/command/', '/command/time']:
            with self.subTest(path=path):
                self.assertEqual(server.request(path)[0], 200)
        for path, target in [('/command', '/command/'), ('/command/index', '/command/'), ('/index', '/')]:
            with self.subTest(path=path):
                status, headers, _ = server.request(path)
                self.assertEqual(status, 301)
                self.assertEqual(headers['location'], target)
        for path in ['/function/', '/command/time/', '/command/time.md', '/command/time.MD', '/index.md', '/private.txt', '/assets/sample.md', '/assets', '/_stories']:
            with self.subTest(path=path):
                self.assertEqual(server.request(path)[0], 404)

    def test_head_parity_and_no_wire_body(self):
        server = self.start()
        for path in ['/', '/command/time', '/time', '/missing', '/function/', '/assets/sample.txt', '/assets/sam%70le.txt', '/%61ssets/sample.txt', '/assets/missing', '/assets/miss%69ng', '/_stories/pico.min.css', '/_stories/%70ico.min.css']:
            with self.subTest(path=path):
                get_status, get_headers, get_body = server.request(path)
                head_status, head_headers, head_body = server.request(path, 'HEAD')
                self.assertEqual(head_status, get_status)
                for key in ['content-length', 'content-type', 'location']:
                    self.assertEqual(head_headers.get(key), get_headers.get(key))
                self.assertEqual(head_body, b'')
                wire_headers, wire_body = server.raw('HEAD', path)
                self.assertIn(str(get_status).encode(), wire_headers.split(b'\r\n', 1)[0])
                self.assertEqual(wire_body, b'')
                self.assertEqual(len(get_body), int(get_headers['content-length']))

    def test_link_rewriting_and_raw_html(self):
        (self.content / 'command' / 'links.md').write_text('[inline](../function/time.md?q=a&b=c#details)\n\n[ref][r]\n\n[r]: index.md\n\n[encoded](time&#46;md)\n\n<a href="unchanged.md">raw</a>\n\n![image](/assets/image.png)\n')
        server = self.start()
        status, _, body = server.request('/command/links')
        self.assertEqual(status, 200)
        self.assertIn(b'href="/function/time?q=a&amp;b=c#details"', body)
        self.assertIn(b'href="/command/"', body)
        self.assertIn(b'href="/command/time"', body)
        self.assertIn(b'<a href="unchanged.md">', body)
        self.assertIn(b'src="/assets/image.png"', body)

    def test_civetweb_static_files_and_ranges(self):
        (self.assets / 'sample.cgi').write_text('#!/bin/sh\nprintf should-not-execute\n')
        (self.assets / 'a b.txt').write_text('space filename')
        server = self.start()
        status, headers, body = server.request('/assets/sample.txt')
        self.assertEqual(status, 200)
        self.assertIn('text/plain', headers['content-type'])
        self.assertEqual(body, b'asset bytes\n')
        status, _, body = server.request('/assets/sample.txt', headers={'Range': 'bytes=0-4'})
        self.assertEqual(status, 206)
        self.assertEqual(body, b'asset')
        status, _, body = server.request('/assets/sam%70le.txt', headers={'Range': 'bytes=0-4'})
        self.assertEqual(status, 206)
        self.assertEqual(body, b'asset')
        status, _, body = server.request('/assets/sam%70le.txt', headers={'If-None-Match': headers['etag']})
        self.assertEqual(status, 304)
        self.assertEqual(body, b'')
        self.assertEqual(server.request('/assets/a%20b.txt')[2], b'space filename')
        self.assertEqual(server.request('/assets/sample.cgi')[2], b'#!/bin/sh\nprintf should-not-execute\n')
        self.assertEqual(server.request('/assets/missing')[0], 404)
        self.assertNotEqual(server.request('/assets/')[0], 200)
        self.assertEqual(server.request('/_stories/pico.min.css')[0], 200)
        self.assertEqual(server.request('/pico.min.css')[0], 404)

    def test_invalid_paths_and_filesystem_boundary(self):
        outside = self.root / 'secret.md'
        outside.write_text('outside secret')
        (self.content / 'link.md').symlink_to(outside)
        (self.content / 'shortcut').symlink_to(self.content / 'command', target_is_directory=True)
        os.mkfifo(self.content / 'fifo.md')
        server = self.start()
        for path in ['/link', '/shortcut/time', '/fifo', '/private.txt', '/assets/../content/index.md', '/assets/%2e%2e/secret', '/command/../index', '/command//time', '/.hidden', '/assets/.hidden']:
            with self.subTest(path=path):
                status, _, body = server.request(path)
                self.assertIn(status, [400, 404])
                self.assertNotIn(b'outside secret', body)
                self.assertNotIn(b'must not leak', body)
        self.assertEqual(server.request('/command/%252e%252e/time')[0], 404)

    def test_encoded_nul_paths_are_rejected(self):
        server = self.start()
        paths = ['/%00', '/index%00junk', '/command/time%00.md', '/time%00_command', '/assets/sample.txt%00junk', '/_stories/pico.min.css%00junk', '/assets/%00/sample.txt']
        for path in paths:
            for method in ['GET', 'HEAD']:
                with self.subTest(path=path, method=method):
                    status, headers, body = server.request(path, method)
                    self.assertEqual(status, 400)
                    self.assertNotIn('location', headers)
                    if method == 'HEAD':
                        self.assertEqual(body, b'')
                        wire_headers, wire_body = server.raw(method, path)
                        self.assertIn(b' 400 ', wire_headers.split(b'\r\n', 1)[0])
                        self.assertEqual(wire_body, b'')

    def test_request_paths_are_decoded_once(self):
        (self.content / 'command' / '%00.md').write_text('# Literal percent zero\n')
        (self.content / 'command' / 'a+b.md').write_text('# Literal plus\n')
        (self.assets / '%00.txt').write_text('literal percent zero')
        (self.assets / 'a+b.txt').write_text('literal plus')
        server = self.start()
        self.assertIn(b'Literal percent zero', server.request('/command/%2500')[2])
        self.assertEqual(server.request('/assets/%2500.txt')[2], b'literal percent zero')
        for path in ['/command/a+b', '/command/a%2Bb']:
            self.assertIn(b'Literal plus', server.request(path)[2])
        for path in ['/assets/a+b.txt', '/assets/a%2Bb.txt']:
            self.assertEqual(server.request(path)[2], b'literal plus')
        status, headers, _ = server.request('/index?value=%00')
        self.assertEqual(status, 301)
        self.assertEqual(headers['location'], '/?value=%00')

    def test_methods(self):
        server = self.start()
        for method in ['POST', 'PUT', 'DELETE', 'OPTIONS', 'PATCH']:
            status, headers, _ = server.request('/command/time', method)
            self.assertEqual(status, 405)
            self.assertEqual(headers['allow'], 'GET, HEAD')

    def test_content_updates_without_reload(self):
        server = self.start()
        self.assertEqual(server.request('/new_command')[0], 404)
        document = self.content / 'command' / 'new.md'
        document.write_text('# Newly added\n')
        self.assertEqual(server.request('/new_command')[1]['location'], '/command/new')
        self.assertIn(b'Newly added', server.request('/command/new')[2])
        document.write_text('# Updated\n')
        self.assertIn(b'Updated', server.request('/command/new')[2])
        document.unlink()
        self.assertEqual(server.request('/command/new')[0], 404)
        (self.assets / 'sample.txt').write_text('updated asset')
        self.assertEqual(server.request('/assets/sample.txt')[2], b'updated asset')

    def test_source_and_render_limits(self):
        (self.content / 'exact.md').write_text('x' * 4096)
        (self.content / 'oversize.md').write_text('x' * 4097)
        (self.content / 'expanded.md').write_text('&' * 1000)
        server = self.start(markdown_limit=4096, output_limit=16384)
        self.assertEqual(server.request('/exact')[0], 200)
        status, _, body = server.request('/oversize')
        self.assertEqual(status, 500)
        self.assertNotIn(b'x' * 100, body)
        self.assertEqual(server.request('/oversize', 'HEAD')[0], 500)
        server.stop()
        server = self.start(markdown_limit=4096, output_limit=2048)
        status, _, body = server.request('/expanded')
        self.assertEqual(status, 500)
        self.assertNotIn(b'&amp;' * 20, body)
        self.assertEqual(server.raw('HEAD', '/expanded')[1], b'')
        self.assertEqual(server.request('/command/time')[0], 200)

    def test_shutdown_under_load(self):
        server = self.start()
        completed = threading.Event()

        def reader():
            successes = 0
            for _ in range(100):
                try:
                    status, _, body = server.request('/command/time')
                except (OSError, http.client.HTTPException):
                    break
                self.assertEqual(status, 200)
                self.assertTrue(body.endswith(b'</html>\n'))
                successes += 1
                completed.set()
            return successes

        with concurrent.futures.ThreadPoolExecutor(max_workers=8) as executor:
            clients = [executor.submit(reader) for _ in range(8)]
            self.assertTrue(completed.wait(timeout=5))
            server.stop()
            self.assertGreater(sum(client.result(timeout=5) for client in clients), 0)
        self.assertEqual(server.process.returncode, 0, server.diagnostics())

    def test_configuration_and_cli(self):
        self.write_config()
        result = subprocess.run([str(BINARY), '--check-config'], cwd=self.root, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        result = subprocess.run([str(BINARY), '--config', 'missing.conf', '--check-config'], cwd=self.root, capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        result = subprocess.run([str(BINARY), '--config', str(self.config), '--threads', '0', '--check-config'], cwd=self.root, capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        result = subprocess.run([str(BINARY), '--config', str(self.config), '--threads', '2', '--check-config'], cwd='/', capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.config.write_text('[unknown]\nbad=true\n')
        result = subprocess.run([str(BINARY), '--check-config'], cwd=self.root, capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)

    def test_failed_startup_and_root_overlap(self):
        self.write_config()
        text = self.config.read_text()
        for invalid in [text.replace('root=content', 'root=missing'), text.replace('[assets]\nroot=assets', '[assets]\nroot=content')]:
            self.config.write_text(invalid)
            result = subprocess.run([str(BINARY), '--config', str(self.config)], capture_output=True, text=True, timeout=5)
            self.assertNotEqual(result.returncode, 0)
        self.config.write_text(text)
        with contextlib.closing(socket.socket()) as occupied:
            occupied.bind(('127.0.0.1', 0))
            occupied.listen()
            endpoint = f'127.0.0.1:{occupied.getsockname()[1]}'
            result = subprocess.run([str(BINARY), '--config', str(self.config), '--listen', endpoint], capture_output=True, text=True, timeout=5)
            self.assertNotEqual(result.returncode, 0)


if __name__ == '__main__':
    unittest.main(verbosity=2)
