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

Milestones 0–3 are implemented:

- **Daemon lifecycle** — auto-spawns `nipa serve` when no discovery file
  exists, waits for readiness, reconnects when the daemon disappears, and
  shuts down only a daemon it started itself.
- **Async transport** — a persistent gRPC channel with token auth; every call
  runs off the UI thread and reports back as a Qt signal.
- **Repositories & tree** — the daemon's `ListRepos` registry plus the
  recursive branch manifest (`ProxyTreeManifest`, scoped by the clone's sparse
  prefixes) shown as a lazily expanded tree with size/binary/read-only hints.
- **Pending changes** — a P4V-style status table (staged/modified/untracked/
  missing/deleted/conflicts, detached-HEAD aware) fed by the daemon's cached
  `Status`, with category filtering, polling and Stage/Unstage from both the
  pending list and the tree.
- **Submit & Update** — a description dialog for `Push` and streamed progress
  with cancel for both `Push` and `Update` (`OpEvent` queue/phase/object/byte
  events), then an automatic status and tree refresh.
- **History** — paginated commit log (`ProxyCommitLog` cursor via the last
  commit id), commit detail with author/message/parents and the tree at that
  commit, plus commit and per-file commit diffs.
- **Diff viewer** — streamed `Diff` output with per-line coloring, working /
  staged / revision comparisons, `patch`/`stat`/`name_only`/`name_status`
  formats, whitespace and context options, cancellation and export.
- **Locks** — binary lock list (scope, holder, acquisition time), lock/unlock
  from the tree and locks tab, and a warning when staging/submitting paths
  someone else has locked.
- **Branches & merge** — branch table (head, default, protected) with create,
  delete and streamed switch; merge with fast-forward policy and message, and
  a conflict flow offering abort or keep-and-resolve.
- **Revision graph** — newest-first, both-parents walk (`ProxyCommitWalk`) from
  the branch head rendered as a lane graph (`●│`) with commit, message and
  date; double-click opens the commit in History.

Next: merge requests, revert, sparse-checkout editor and login (M4). The full
roadmap (ACLs, P4V parity) is tracked in
[INTEGRATIONS.md](https://github.com/nipalab/nipa/blob/main/docs/INTEGRATIONS.md).

## License

Apache License 2.0 — see [LICENSE](LICENSE).
