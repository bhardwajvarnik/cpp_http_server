# cpp-http-server

A multi-threaded HTTP/1.1 server written from raw POSIX sockets in modern C++ —
no Boost.Asio, no third-party HTTP libraries. It can run as a standalone web
server (routing, static files, keep-alive) or as a reverse proxy / load
balancer in front of other instances of itself.

Built as a systems-programming portfolio project to demonstrate networking,
concurrency, and performance-oriented C++.

## Features

- **Raw socket implementation** — `socket()`/`bind()`/`listen()`/`accept()` directly, no framework
- **Thread pool** — bounded worker pool handles connections concurrently instead of blocking on one accept loop
- **HTTP/1.1 request parsing** — method, path, headers, body (including `Content-Length`-aware body reads)
- **Keep-alive** — persistent connections, multiple requests per socket, correct `Connection: close` handling and a request-count cap per connection
- **Routing** — register handlers per method + path, plus static file serving with MIME-type detection and directory-traversal protection
- **Reverse proxy / load balancer mode** — round-robin or least-connections strategy across N backends, with a background health-check thread that skips dead nodes
- **Performance touches** — `TCP_NODELAY` to avoid Nagle's algorithm latency, `SO_REUSEADDR` for fast restarts, read timeouts to avoid resource exhaustion from idle clients

## Architecture

```
                 ┌─────────────────┐
   TCP conns ──▶ │  accept() loop   │
                 └────────┬─────────┘
                          │ enqueue(client_fd)
                          ▼
                 ┌─────────────────┐
                 │   Thread Pool    │  (N worker threads)
                 └────────┬─────────┘
                          │
              ┌───────────┴────────────┐
              ▼                        ▼
     ┌─────────────────┐     ┌──────────────────┐
     │  HTTP parsing +  │     │  LoadBalancer     │
     │  Router::route() │ or  │  ::forward()      │
     └─────────────────┘     └────────┬──────────┘
                                       │ round-robin /
                                       │ least-connections
                              ┌────────┴────────┐
                              ▼                 ▼
                        backend :9001     backend :9002
```

Each accepted connection is handed to a worker thread, which owns that
connection for its whole lifetime (including keep-alive re-reads), rather
than the server spawning a thread per request or per connection.

## Build

Requires CMake ≥ 3.16, a C++ compiler, and pthreads (Linux/macOS/WSL).

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j
```

## Run

**As a standalone server:**
```bash
./build/server --port 8080 --threads 4 --static ./public
```

**As a reverse proxy / load balancer** (start backends first, e.g. two more
instances of this same server on different ports):
```bash
./build/server --port 9001 &
./build/server --port 9002 &
./build/server --port 8080 --lb round-robin --backend 127.0.0.1:9001 --backend 127.0.0.1:9002
```

```
Usage:
  ./server --port 8080 --threads 4 [--static ./public]
  ./server --port 8080 --lb round-robin --backend 127.0.0.1:9001 --backend 127.0.0.1:9002

Options:
  --port <n>          Listen port (default 8080)
  --threads <n>       Worker thread pool size (default: hardware concurrency)
  --static <dir>      Serve static files under /static from this directory
  --lb [strategy]     Enable reverse-proxy mode. strategy = round-robin (default) | least-connections
  --backend host:port Add a backend (repeatable, only used with --lb)
```

## Try it

```bash
curl http://localhost:8080/
curl http://localhost:8080/health
curl -X POST -d 'hello world' http://localhost:8080/echo
curl http://localhost:8080/static/
```

## Tests

Unit tests cover the HTTP parser (request line parsing, header parsing,
keep-alive semantics, malformed input rejection) and the thread pool
(task-completion correctness under concurrency).

```bash
cd build && ctest --output-on-failure
```

CI (`.github/workflows/ci.yml`) builds the project and runs the full test
suite, plus a smoke test against the actual running binary, on every push.

## Benchmarking

```bash
sudo apt-get install wrk
./scripts/benchmark.sh 8080 10s 4 100
```




