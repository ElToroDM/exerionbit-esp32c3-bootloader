# Compliance Baseline — Waveshare ESP32-C3 Zero Custom Bootloader

**Version:** v0.2  
**Date:** 2026-03-23

---

## Purpose

This note captures the compliance-ready baseline documented by this ESP32-C3 bootloader repository.
It is intentionally narrow: reproducible boot behavior, explicit handoff, and a visible
security contact path. It is not a claim of full production hardening.

## Scope

Included in this repository baseline:
- deterministic boot decision behavior on Waveshare ESP32-C3 Zero
- baseline integrity signaling and validation flow
- reproducible evidence artifacts for normal and recovery/update paths
- BRS-B principles alignment wording with an explicit non-conformance note

Out of scope:
- advanced production hardening internals
- full key lifecycle architecture
- anti-tamper implementation details
- authenticated image signing or encryption in this repository baseline

## Baseline assumptions

- Physical ESP32-C3 hardware is required for live validation.
- Early USB CDC output may be hidden during reset; evidence capture may require watcher tooling.
- The published evidence set documents baseline boot behavior, not production hardening.
- Advanced platform security features are outside the scope of this repository evidence set.

## Vulnerability handling path

For non-sensitive issues:
- open a GitHub issue in this repository with prefix `[SECURITY]`
- include reproduction steps, expected behavior, observed behavior, and environment details

For potentially sensitive findings:
- do not post exploit details publicly
- open a brief contact request via GitHub issue without exploit details
- request a secure follow-up channel

## Support window

Support in this repository is limited to:
- reproducibility issues in documented setup and validation paths
- correctness issues in published baseline behavior

Separate agreements can define explicit response windows and acceptance criteria.

## Known limits

- Live validation requires physical hardware access.
- Native USB re-enumeration can hide early boot lines.
- No OTA slot scheme is included in this repository baseline.
- No rollback protection or production fault-injection hardening is included.
- No production key provisioning is included.

See [docs/KNOWN_LIMITS.md](../../KNOWN_LIMITS.md) for the full limit set.

## BRS-B conformance note

This asset is aligned with BRS-B principles: minimal boot path, explicit handoff, and no heavy UEFI/ACPI dependency.
That wording is a baseline alignment statement, not a full BRS/BRS-B conformance claim.

Reference source and last checked date:
- RISC-V BRS ratification (2025)
- last checked: 2026-03-18

## Evidence mapping

This baseline is supported by the repository documentation and evidence set:
- [README.md](../../../README.md)
- [SECURITY.md](../../../SECURITY.md)
- [BOOT_SEQUENCE.md](../../../BOOT_SEQUENCE.md)
- [VALIDATION_PROFILE.md](../../../VALIDATION_PROFILE.md)
- [docs/KNOWN_LIMITS.md](../../KNOWN_LIMITS.md)
- [docs/evidence/v0.2/expected-vs-observed.md](expected-vs-observed.md)

---

*This note describes the documented repository baseline. Production hardening is outside the scope of this note.*
