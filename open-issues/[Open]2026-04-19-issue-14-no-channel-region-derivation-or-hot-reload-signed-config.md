# Issue 14 — No channel/region derivation, no hot-reload, no signed config

**Status:** Open.
**Observed:** 2026-04-19, rcc C++ port architecture review.

## Symptom

Architecture §13 "Channel Index -> Frequency Mapping" requires:

1. Expanding `supported_frequency_profiles` range specs into explicit frequency lists.
2. Intersecting with a regional constraint config.
3. Producing a sorted, deduplicated, 1-based channel set.
4. Republishing on capability or region change.

Architecture §"Configuration (Region/Channel Policy)" further requires a "signed region config" and hot-reload with signature verification.

Current state:
- [silvus_adapter.cpp](services/radio-control/rcc/src/adapter/silvus_adapter.cpp) hard-codes `{2412, 2437, 2462}` MHz as the initial capability and uses whatever `supported_frequency_profiles` returns at `connect()` time.
- There is no region config file, no derivation step, no signature check, and no hot-reload — `ConfigManager` reads YAML once at startup.

## Why it matters

- Illegal-frequency blocking is only as good as the radio's own response; the container has no regional policy to refuse values the radio would accept.
- Field redeployment across regions requires a container rebuild today.
- Channel numbering may drift from what the Android UI expects if a radio advertises a superset of legal frequencies.

## Fix (sketch)

1. Add a `ChannelMapper` component that takes `{supported_frequencies, region_policy}` and produces the 1-based channel set used by `Orchestrator` and `/radios` responses.
2. Load a signed region config (e.g. detached x509 signature over a YAML blob) under `/etc/rcc/region.yaml.sig`; refuse to start if signature fails.
3. Expose a reload trigger (signal or admin endpoint) that re-verifies the signature, re-derives the channel set, and publishes a change event on SSE.

## Related

- Issue 12 (capability refresh on re-attach) — the same derivation pipeline is triggered by both sources of capability change.
