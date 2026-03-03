# Security Policy

## Scope

This public repository provides a minimal, reproducible bootloader baseline for ESP32-C3.

Included in scope:
- deterministic boot decision behavior
- baseline integrity signaling and validation flow
- reproducible evidence artifacts

Out of scope:
- advanced production hardening internals
- full key lifecycle architecture
- anti-tamper implementation details

## Reporting a Vulnerability

For non-sensitive security issues:
- open a GitHub issue in this repository with prefix `[SECURITY]`
- include reproduction steps, expected behavior, observed behavior, and environment details

For potentially sensitive findings:
- do not post exploit details publicly
- open a minimal private contact request via GitHub issue (without exploit details) and request a secure follow-up channel

## Response Expectations

Target response times:
- acknowledgement: within 5 business days
- triage update: within 10 business days
- resolution plan (or scope decision): within 20 business days

These targets are best-effort for a solo-maintained repository and may vary with hardware availability.

## Support Window

Public baseline support is best-effort and scoped to:
- reproducibility issues in documented setup/validation paths
- correctness issues in published baseline behavior

This repository does not provide guaranteed SLA support. Paid engagements can define explicit response windows and acceptance criteria.
