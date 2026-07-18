# Changelog

_All notable changes to this project. Newest first._
_Format loosely follows [Keep a Changelog](https://keepachangelog.com/)._

## [Unreleased]
### Added
- Host foundation code (C++17, WSL2-built): Result/Error, EventBus, RingBuffer, SHA-256 (NIST
  vectors), and the plugin-ready ScreenPlugin interface + PluginScheduler playlist — 17 unit
  tests, 371 checks, all passing.
- Modular smart-display reframe (InkyPi-inspired): ADR-008 (ScreenPlugin architecture),
  ADR-009 (shared TLS+JSON layer); roadmap M4 (local plugins) + M5 (network plugins). Photo-first.
- Test plan (TESTPLAN.md): 48 traceable test rows (P0 safety / P1 core / P2 delight) mapped from
  scenarios and spikes, with pass criteria, types (unit/target/HIL), and a test-first order.
- Behavioral scenario catalog (docs/SCENARIOS.md): 50+ Given/When/Then scenarios for the deployed
  product (online/offline, power loss, updates, network, faults, security, recovery) → feeds tests.
- Deep subsystem design (docs/DESIGN.md): boot/image layout, content pipeline, render engine,
  and OTA/A-B update — with Circle APIs, ported libs, and the dependent spikes.
- Project scaffolding: CLAUDE.md, guardrails, goals/roadmap/decisions/outcomes docs,
  test plan, retrospective & learnings logs.
- Product & engineering spec (docs/PRD.md) for the Lumen Frame digital photo frame.
- Filled in Goals, Roadmap, and Decisions (ADR-001..003) from the planning session.

### Changed
- **Major pivot:** re-architected from a Linux appliance to a **genuine bare-metal firmware OS
  in C++ on Circle** (no Linux). Rewrote PRD, roadmap, and goals; superseded ADR-001..003 with
  ADR-004..007 (Circle bare-metal, C++ software renderer, HEIC boundary transcode).

### Fixed
- _..._
