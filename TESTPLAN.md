# Test Plan

_What we test, how, and the pass/fail bar. Ties back to success criteria in
[docs/GOALS.md](docs/GOALS.md)._

## How to run tests
- Unit tests: `<command>`
- On-target / integration: `<procedure>`
- Manual/HIL checks: `<steps>`

## Test matrix
| ID | What it verifies | Type | How | Pass criteria | Status |
|----|------------------|------|-----|----------------|--------|
| T1 | Firmware builds cleanly | Build | run build command | 0 errors/warnings | ⬜ |
| T2 | Boots on target | Smoke | flash + observe | reaches main loop | ⬜ |
| T3 | _Feature A behaves_ | Unit | _..._ | _..._ | ⬜ |
| T4 | _Edge case / fault_ | Robustness | _..._ | recovers gracefully | ⬜ |

_Status legend: ⬜ not run · 🟡 partial · ✅ pass · ❌ fail_

## Known gaps
_Things we know aren't covered yet — be honest so nobody assumes they are._
- _..._
