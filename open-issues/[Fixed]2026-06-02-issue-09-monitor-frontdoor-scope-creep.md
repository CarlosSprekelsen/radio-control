# Issue 09 - Monitor front-door scope creep / port-80 mapping

**Status:** Fixed.
**Reviewed:** 2026-06-02.

## Resolution

The current LXC package setup generates a minimal nginx front door from the RCC
service config:

- Reads `/etc/rcc/config.yaml` and derives `network.command_port`, defaulting to
  `8080`.
- Proxies only `/` and `/monitor` to `127.0.0.1:<REST_PORT>/monitor`.
- Does not proxy API or SSE through nginx.
- Removes `sites-enabled/default`.
- Installs `sites-enabled/rcc`.

The Podman path does not use nginx and exposes the REST/monitor port directly.

## Evidence

- `deployment/lxc/build-package.sh` dynamic nginx block.
- `deployment/podman/deploy.sh` and `deployment/podman/README.md` expose
  REST/monitor on `8080` and SSE on `8081`.

## Residual Risk

Live LXC validation should still confirm `http://192.168.101.36/monitor` on the
target hub image, because this review did not run the LXC package build.
