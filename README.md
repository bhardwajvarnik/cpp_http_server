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

Requires CMake ≥ 3.16, a C++17 compiler, and pthreads (Linux/macOS/WSL).

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

This reports requests/sec and latency percentiles — worth including as a
number in your resume bullet once you've run it on your own machine
(e.g. "sustained ~X req/s with Y ms p99 latency under 100 concurrent
connections").

## Known simplifications (good talking points in an interview)

- The reverse proxy re-wraps the backend's response body rather than
  forwarding its headers byte-for-byte — a production proxy would pass
  headers through untouched.
- Uses a blocking-read-per-thread model (thread pool), not an `epoll`-based
  event loop. This is simpler to reason about and still handles real
  concurrent load well; an `epoll`/reactor rewrite (nginx-style, one loop
  per thread) is the natural "next stage" if you want to push scalability
  further and is a good thing to mention as future work.
- No HTTPS/TLS termination — would typically sit behind a TLS-terminating
  proxy or use OpenSSL directly.

## Suggested resume bullet

> Built a multi-threaded HTTP/1.1 server and reverse proxy in C++17 from raw
> POSIX sockets (no framework), implementing a custom thread pool, HTTP
> parser, keep-alive connection handling, and round-robin/least-connections
> load balancing with health checks; validated with a unit test suite and
> CI, and benchmarked at [X] req/s.

## License

MIT — see [LICENSE](LICENSE).
