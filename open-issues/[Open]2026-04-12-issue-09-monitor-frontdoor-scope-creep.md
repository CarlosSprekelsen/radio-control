# Issue 09 — Monitor front-door scope creep / port-80 mapping unverified

**Status:** Open.
**Observed:** 2026-04-12, cross-service audit.

## Symptom

[services/radio-control/deploy/lxc/setup.sh](services/radio-control/deploy/lxc/setup.sh) contains nginx configuration but `http://192.168.101.36/` is not known to serve the monitor page, and the vhost almost certainly replicates the scope-creep pattern found (and now fixed) in `biometric-control`: API/SSE proxied through nginx, hard-coded backend ports that drift from `config.yaml` `apiPort`, possibly TLS enabled without certs.

## Fix

Same remediation as biometric-control issue 13 / lrf-control issue 02:

- Reduce vhost to minimal gpsd-proxy pattern (only `/` and `/monitor` → `127.0.0.1:<REST_PORT>/monitor`, no TLS, no API/SSE proxy).
- Ensure `sites-enabled/` symlink exists and `sites-enabled/default` removed.
- Set `tlsEnabled: false` in the shipped config.

Confirm backend REST port by reading `services/radio-control/config/*.yaml` for `network.apiPort` (REST = apiPort + 1 by watchdog convention).

## Related

- Existing issue 08 (`radio-service-missing-port-allocation-and-api-contracts`) — fixing port allocation first may change the backend target used by this vhost.
- biometric-control issue 13, gpsd-proxy reference.
