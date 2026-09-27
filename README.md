# nipa-gui

P4V-style desktop client for [nipa](https://github.com/nipalab/nipa), the
centralized version control system for binary-heavy projects.

`nipa-gui` is a thin Qt view over the local **nipa daemon**: it owns no VCS
logic. Every operation goes through the loopback gRPC API served by
`nipa serve` (branching, status, staging, push/update, diffs, merge requests,
file locks), so the GUI and the CLI speak exactly the same protocol.

## How it connects

1. `nipa serve` publishes its loopback endpoint in the user config directory:
   `~/.config/nipa/daemon.json` (`{pid, port, token, version}`, 0600).
2. The client dials `127.0.0.1:<port>` and sends the capability token as
   `x-nipa-daemon-token` metadata on every RPC.
3. `Ping` reports the daemon version and pid. A client should refuse to start
   (or warn) when the daemon is older than the API it was built against.

The daemon protocol snapshot lives in `proto/` (see the sync note at the top of
each file). Design docs: [DAEMON.md](https://github.com/nipalab/nipa/blob/main/docs/DAEMON.md),
[INTEGRATIONS.md](https://github.com/nipalab/nipa/blob/main/docs/INTEGRATIONS.md).

## Build

Prerequisites: CMake ≥ 3.24, a C++20 compiler, Qt 6 Widgets, gRPC and protobuf
development packages.

Debian/Ubuntu:

```sh
sudo apt-get install cmake ninja-build qt6-base-dev \
    libprotobuf-dev protobuf-compiler protobuf-compiler-grpc libgrpc++-dev
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/nipa-gui
```

## Status

Walking skeleton: daemon discovery, token auth and `Ping`. The feature roadmap
(workspace/depot tree, status, stage/submit, history, locks, merge requests,
full P4V parity) is tracked in
[INTEGRATIONS.md](https://github.com/nipalab/nipa/blob/main/docs/INTEGRATIONS.md).

## License

Apache License 2.0 — see [LICENSE](LICENSE).
