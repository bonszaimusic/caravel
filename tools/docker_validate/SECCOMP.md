<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Native ThreadSanitizer

GCC ThreadSanitizer on ARM64 disables process address randomization at startup.
Docker's default seccomp profile rejects `personality(ADDR_NO_RANDOMIZE)`; this
produces a TSAN runtime CHECK failure before the test starts, rather than a data
race report.

`seccomp-tsan.json` is the Moby default profile from
[`moby/profiles` commit a0fa70805f742f7da15b367770d802ea2a86c68b](https://github.com/moby/profiles/blob/a0fa70805f742f7da15b367770d802ea2a86c68b/seccomp/default.json),
with one additional allow rule: syscall `personality`, argument 0 exactly
`262144` (`ADDR_NO_RANDOMIZE`). All other upstream rules are retained. The
upstream Apache-2.0 license is in `LICENSE.seccomp`.

Only the native `cpu_dual` container uses this profile. It adds no capabilities,
does not disable seccomp, and does not change the Docker daemon configuration.
TSAN failures remain fatal. Explicit emulated baseline runs retain the recorded
historical TSAN limitation; the runner never falls back to emulation on failure.
