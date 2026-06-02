# [Open] radio-control: derived Silvus ICD needs page-level vendor traceability

## Status

Open. Documentation validation gap.

## Owner

`docs/swad-dts/specifications`.

## Deviation

`docs/swad-dts/specifications/icd-radio-silvus-derived.md` contains a useful
reduced StreamCaster ICD, but the command tables do not cite vendor manual page
or section numbers for each retained command. During the 2026-06-02 review, the
local PDF could not be machine-extracted with the available shell tools, so the
service and mock were validated against the derived ICD text and existing
cross-doc references, not page-anchored vendor excerpts.

Small transcription defects were fixed in this pass (`/streamscape_api` endpoint
placeholder and the `error` string/object wording), but full auditability still
requires page-level trace links.

## Why It Matters

The reduced ICD is intended to be derived from the vendor ICD. Without page-level
traceability, future implementers cannot quickly distinguish vendor facts from
DTS policy choices, copied examples, or lab-observed behavior.

## Required Work

1. Add page/section references from `StreamCaster API Manual (SILVUS).pdf` for
   each command retained by the derived ICD.
2. Mark whether each row is vendor-documented, lab-observed, or DTS policy.
3. Attach or generate a searchable text export of the vendor manual for review
   workflows, if licensing permits.
4. Re-run the derived ICD review after StreamCaster 4000 lab captures are
   available.

## Acceptance Criteria

- Every reduced-ICD command has a vendor page/section reference or an explicit
  lab/TBD marker.
- DTS-specific transformations, such as channel derivation and regional
  filtering, are clearly separated from Silvus API behavior.
- Reviewers can reproduce the ICD comparison without manual PDF archaeology.

## Cross-References

- `services/radio-control/docs/StreamCaster API Manual (SILVUS).pdf`
- `docs/swad-dts/specifications/icd-radio-silvus-derived.md`
