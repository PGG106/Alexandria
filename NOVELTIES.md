# Alexandria Novelty Tracker

This tracker records isolated experiments sourced exclusively from Obsidian.

## Workflow

- Every experiment branches directly from `master` and contains one behavioral change.
- A bench validates deterministic behavior; it is not an Elo result.
- `processed.net` and other generated network artifacts are never committed.
- The user runs SPRT externally.

## Results

| # | Novelty | Commit | Bench | SPRT / result | Status |
|---|---|---|---:|---|---|
| 1 | Minor/major correction history | n/a | n/a | STC -0.01 ± 1.41; LTC -2.81 ± 3.04 | FAILED / REVERTED |
| 2 | Continuation-history ply 3 | reverted | 7,729,162 | Not Obsidian-consistent | REVERTED |
| 3 | Capture-split continuation history | `049b24c` | 8,048,082 | STC +0.51 ± 0.80; LLR -2.27 | FAILED / REVERTED |
| 4 | ttMoveNoisy reductions | `2fd44db` | 8,212,655 | Historical bundle only | ARCHIVED |
| 5b | TT-depth LMR reduce-less term | `1d46d2b` | 9,850,674 | STC -1.32 ± 2.32; LLR -2.25 | FAILED / REVERTED |
| 14 | TM score-loss scaling | `7efa324` | 8,498,891 | Passed | PASSED |
| 18 | Half-weight continuation-history updates at plies 4 and 6 | pending | 8,331,042 | Awaiting SPRT | TESTING (ISOLATED, FROM MASTER) |

## Current Experiment: #18 — Half-weight deep continuation-history updates

Obsidian updates continuation history at offsets 1, 2, 4, and 6. Offsets 1 and 2 receive the full bonus; offsets 4 and 6 receive `bonus / 2`. Alexandria previously applied the full bonus at every offset.

- Branch: `ch-deep-halfweight-iso`
- Base: `master` (`2ebf114`)
- Scope: `updateCHScore()` only.
- Validation: release and assertion-enabled debug benches both completed with 8,331,042 nodes.
- Status: awaiting external SPRT.

## Selected Backlog

| # | Novelty | Status |
|---|---|---|
| 6 | Threat-based quiet move ordering | UNTESTED |
| 7 | Promotion bonus in capture ordering | UNTESTED |
| 8 | Stronger singular negative extension | UNTESTED |
| 9 | PV/cut-node IIR condition | UNTESTED |
| 10 | NMP gated on cut nodes | UNTESTED |
| 11 | Razoring depth policy | UNTESTED |
| 12 | RFP margin floor | UNTESTED |
| 13 | Three-way continuation-history update after LMR re-search | UNTESTED |
| 15 | TM one-legal-move early stop | UNTESTED |
| 16 | TM timing constants | UNTESTED |
| 17 | Configurable move overhead | UNTESTED |
| 19 | Correction-history grain | UNTESTED |
| 20 | Richer continuation correction-history index | UNTESTED |
| 21 | Evaluation material scaling | UNTESTED / HIGH RISK |
| 25 | Quiet checks in qsearch | UNTESTED |
| 26 | Bucketed 50-move-rule TT keys | UNTESTED |
| 27 | Contempt options | UNTESTED / LOW PRIORITY |
| 28 | PGO build target | UNTESTED / BUILD |
| 29 | Parallel transposition-table clear | UNTESTED / LOW PRIORITY |

## Scope

- Idea source: Obsidian only (`github.com/gab8192/Obsidian`).
- Baseline: `master` at `2ebf114`.
- Typical external tests: STC 8.0+0.08, Threads=1, Hash=16 MB; LTC 40.0+0.40, Threads=1, Hash=64 MB.
