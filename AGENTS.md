# Remote Machine Safety

The remote Kali machine exposed through SSH/ngrok is a GOLDEN STATE reference system.

Rules:
- Treat the remote Kali machine as read-only.
- Do not modify files on the remote Kali machine.
- Do not install, upgrade, remove, or reconfigure packages, tools, shells, PATH, or dependencies on the remote Kali machine.
- Do not run commands on the remote Kali machine that change system state.
- Use the remote Kali machine only for read-only inspection, version checks, and non-mutating build or diagnostic commands.
- Treat the remote Kali machine as the source of truth when comparing behavior against the local machine.

## SSH Access

- Current temporary SSH endpoint:
  - Host: `2.tcp.ngrok.io`
  - Port: `11608`
  - Username: `ethanhaaan`
- Authentication is SSH key only.
- Approved local key for access: `~/.ssh/id_ed25519`
- Preferred connection command:
  - `ssh -i ~/.ssh/id_ed25519 -p 11608 ethanhaaan@2.tcp.ngrok.io`
- This ngrok host and port are temporary. If the connection fails, ask the user for the latest SSH host and port instead of trying to reconfigure anything.
- Do not add, remove, rotate, or manage keys on the remote machine. Key management remains user-controlled.

## Remote Shell Behavior

- Prefer remote commands wrapped in login bash:
  - `ssh -i ~/.ssh/id_ed25519 -p 11608 ethanhaaan@2.tcp.ngrok.io 'bash -lic "<command>"'`
- Reason: the working remote environment loads `~/.local/bin` in login bash, which is required for the remote QMK tooling to resolve correctly.
- Do not assume non-login shells or `zsh` on the remote machine have the same PATH or QMK behavior.

## Correct Remote Repository

- The correct source-of-truth repository on the remote machine is:
  - `~/Desktop/pcb/qmk_firmware`
- Use that repository for all remote comparisons, build checks, and file inspection.
- Do not use stale or unrelated paths such as:
  - `~/qmk_firmware`
  - `~/Desktop/qmk_firmware`
- The verified remote branch for the source-of-truth repo is:
  - `firmware-highspeed-refactor`

## Remote Build Verification

- For read-only remote build checks, prefer running from the correct repo inside login bash.
- Verified example pattern:
  - `ssh -i ~/.ssh/id_ed25519 -p 11608 ethanhaaan@2.tcp.ngrok.io 'bash -lic "cd ~/Desktop/pcb/qmk_firmware && make toffee_studio/module:via -j1"'`
- Keep all remote verification non-mutating and scoped to inspection or compilation only.
