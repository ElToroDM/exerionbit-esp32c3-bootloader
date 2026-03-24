# Security Policy

## Scope

This repository provides a minimal, reproducible bootloader baseline for ESP32-C3.

**Standards note:** this asset is aligned with BRS-B principles (minimal boot path, explicit handoff, no heavy UEFI/ACPI dependency), but it is not a full BRS/BRS-B conformance claim.

Included in scope:
- deterministic boot decision behavior
- baseline integrity signaling and validation flow (CRC-32/ISO-HDLC app payload validation)
- reproducible evidence artifacts
- CRC validation for app image integrity checks

Out of scope:
- advanced production hardening details (e.g., key provisioning, tamper response)
- full key lifecycle architecture
- anti-tamper implementation details
- authenticated image signing or encryption (baseline uses CRC checksums only)

## Reporting a Vulnerability

For non-sensitive security issues:
- open a GitHub issue in this repository with prefix `[SECURITY]`
- include reproduction steps, expected behavior, observed behavior, and environment details

For potentially sensitive findings:
- do not post exploit details publicly
- open a brief contact request via GitHub issue (without exploit details) and request a secure follow-up channel

## Response Model

- published setup and validation paths are the primary focus for repository issue handling
- documented behavior regressions are the primary focus for repository issue handling
- additional delivery expectations belong in a separate project scope, not in this repository

## Support Window

Support in this repository is limited to:
- reproducibility issues in documented setup/validation paths
- correctness issues in published baseline behavior

Separate project scopes can define explicit response windows and acceptance criteria.
