# Issue 13 - Audit log now uses dts-common rotated file sink

**Status:** Fixed.
**Observed:** 2026-04-19, rcc C++ port architecture review.
**Fixed:** 2026-06-02.

## Original Symptom

`rcc/src/audit/audit_logger.cpp` emitted records only through
`dts::common::core::getLogger().info("[AUDIT] ...")`, so audit events were
visible on stdout/journald but were not persisted to a bounded local audit file.

## Fix

RCC now wraps `dts::common::security::AuditLogger` for control-action audit
records. The shared logger:

- writes append-only JSON audit records to a configured file,
- mirrors those records to the normal `dts-common` log path for operator
  visibility,
- redacts sensitive fields before writing, and
- rotates the active audit file by size with bounded retention.

RCC exposes the sink through config:

```yaml
audit:
  enabled: true
  file_path: "/var/log/rcc/audit.log"
  rotate_after_bytes: 1048576
  rotated_file_count: 10
```

## Evidence

- `libs/dts-common/include/dts/common/security/audit_logger.hpp`
- `libs/dts-common/test/unit/test_audit_logger.cpp`
- `services/radio-control/rcc/src/audit/audit_logger.cpp`
- `services/radio-control/rcc/config/default.yaml`
- `services/radio-control/rcc/test/unit/test_audit_logger.cpp`

## Residual Notes

Local C++ test execution was not available on the 2026-06-02 Windows review host
because CMake selected NMake and neither `nmake` nor an alternate compiler
generator was installed. The change is covered by added unit tests for the next
Linux/CI build.
