# Tungsten Gamma Attenuation Benchmarks

*(English follows the Korean summary.)*

## 개요
이 저장소에는 텅스텐 감마 감쇠를 다루는 두 개의 Geant4 애플리케이션이 포함되어 있습니다.

| 하위 폴더 | 목적 | 지오메트리 | 추가 기능 |
|----------|------|-------------|-----------|
| `basic/` | 순수 박막 벤치마크 | 진공 속 단일 텅스텐 박막 | 결정적 시드, CPE 보정 μ<sub>en</sub>/ρ, 플로팅/리포트 스크립트 |
| `variants/` | 백킹 조절 기능이 있는 확장형 벤치마크 | 박막 + 선택적 텅스텐 백킹 | `/det/setBackingThickness`, 박막/백킹 비교 매크로, CSV에 백킹 두께 기록 |

공통 사항:
- HybridEmPhysics (Option4) 물리 리스트 사용.
-,CSV 스키마 및 `plot_coefficients.py`/`generate_nist_error.py` 워크플로 공유.
- 플롯에는 시뮬레이션 곡선과 NIST 곡선/포인트, 잔차 플롯이 함께 표시됩니다.

### 왜 μ<sub>en</sub>/ρ (raw)가 NIST와 크게 다른가?
얇은 박막에서는 충전입자 평형(CPE)이 성립하지 않아 2차 전자가 포일 밖으로 도달하기 전에 사라집니다. `RunAction`은 이 에너지 손실을 그대로 기록하기 때문에 `(μ_en/ρ)_raw`는 NIST 정의(완전 평형 조건)보다 훨씬 작습니다. 대신 `(μ_en/ρ)_CPE = (μ/ρ)_counts × (μ_en/μ)_NIST` (또는 계산기의 `μ_tr/ρ`)를 보고하여, 백킹 등을 통해 얻을 수 있는 이상적인 평형 상황을 근사합니다.

This repository tracks two closely related Geant4 applications that estimate tungsten attenuation coefficients from transmission simulations:

| Subtree  | Purpose | Geometry | Notable extras |
|----------|---------|----------|----------------|
| `basic/` | Minimal “bare foil” benchmark | Single tungsten foil in vacuum | Deterministic seeds, CPE-adjusted μ<sub>en</sub>/ρ, plotting/report scripts |
| `variants/` | Extended benchmark with downstream backing control | Foil plus optional tungsten backing plate | `/det/setBackingThickness` UI command, foil-only vs backed macros, extra CSV column |

Both share the same HybridEmPhysics (Option4) list, diagnostic logging, CSV schema, and plotting utilities; only the geometry and macros differ. The rest of this document summarizes the layout for teammates and future Git history.

---

## Repository Layout

```
alpha/
├── basic/                 # Bare-foil benchmark application
│   ├── include/, src/     # Geant4 user classes (Detector*, Run*, etc.)
│   ├── mac/               # Deterministic macros (benchmark, energy grids, scans)
│   ├── plot_coefficients.py
│   ├── generate_nist_error.py
│   └── nist_reference.csv # Canonical NIST/XCOM data (renamed for clarity)
├── variants/              # Foil+backing benchmark
│   ├── include/, src/
│   ├── mac/ (includes benchmark_compare.mac, benchmark_core.mac)
│   ├── plot_coefficients.py
│   ├── generate_nist_error.py
│   └── nist_reference.csv
├── generate_nist_error.py # Shared helper (copied into each build tree)
└── nist_reference.csv     # Top-level copy for convenience
```

### Why keep both builds?
- **Basic** mirrors the cleanest possible setup for presentations and for validating the raw Geant4 behaviour against NIST tables. It keeps a single foil, no backing, and produces a compact CSV.
- **Variants** adds controls we need during design discussions (e.g., quantify how a backing plate recovers charged-particle equilibrium). It writes an extra `backing_thickness_um` column and provides paired macros (`benchmark_compare.mac`) so foil-only and backed runs share the same CSV.

The `plot_coefficients.py` scripts include the same improvements in both trees: simulation curves plus NIST curves/points and a residual subplot, with per-thickness error tables under `plots/errors/`.

## Quick Answers
- **Do the plots overlay NIST data?** Yes. The updated `plot_coefficients.py` in both trees plots simulation curves and scatters both simulation and NIST points, plus a residual subplot for percentage deltas.
- **Where do I edit the formulas?** Both READMEs now have bilingual equation sections and a “formula glossary” table; update those if the calculation pipeline changes.
- **What if I don’t need the variants build?** You can work inside `basic/` only, but keep `variants/` committed so teammates who rely on the backing comparisons can continue using it.
