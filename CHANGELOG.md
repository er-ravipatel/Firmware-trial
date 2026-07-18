# Changelog

_All notable changes to this project. Newest first._
_Format loosely follows [Keep a Changelog](https://keepachangelog.com/)._

## [Unreleased]
### Added
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
