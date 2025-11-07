이 문서는 한국어와 영어 두 가지 버전으로 제공되며, 아래에 한국어 설명이 먼저 나옵니다.

---

## 한국어 버전

### 개요
variants 빌드는 기본 라인을 확장해 박막 뒤에 텅스텐 백킹(backing) 층을 배치할 수 있도록 했고, 투과 추적 로직을 Foil→Backing→World 경계까지 확장했습니다. 모든 매크로는 `/random/setSeeds 123456 789012`로 난수를 고정하므로, 동일한 매크로를 반복 실행하면 CSV가 완전히 재현됩니다.

### 물리 계산과 사용된 식
기본 빌드와 동일한 식을 사용하되, 백킹 두께를 추가로 기록합니다.

1. **투과 기반 감쇠**
   ```
   T_counts      = N_uncollided / N_injected
   μ_counts/mm   = -ln(T_counts) / thickness_mm
   (μ/ρ)_counts  = (μ_counts/mm × 10) / density_g_cm3
   ```

2. **에너지 흡수 및 CPE 보정**
   ```
   (μ_en/ρ)_raw = (E_dep / (E0 × N_injected)) / (ρ × thickness_cm)
   ratio_ref    = (μ_en/ρ)_ref / (μ/ρ)_ref
   (μ_en/ρ)_CPE = (μ/ρ)_counts × ratio_ref   [참조 없으면 μ_tr/ρ 사용]
   ```

3. **유효 감쇠 계수**
   ```
   μ_eff/mm = -ln(1 - absorbedFraction) / thickness_mm
   μ_eff/ρ  = (μ_eff/mm × 10) / ρ
   ```

4. **보조 계산**
   - `G4EmCalculator`가 포토/콤프턴/레이리/쌍생성 단면을 합산해 `μ_calc`를 만들고, 콤프턴 잔류 적분을 통해 `μ_tr`을 제공합니다.
   - 백킹을 두꺼운 텅스텐 slab로 둘 경우, `SteppingAction`은 Foil→Backing→World 경계를 추적해 동일 식을 적용합니다.

| Quantity        | Definition / Notes                                             | Units |
|-----------------|----------------------------------------------------------------|-------|
| `T_counts`      | `N_uncollided / N_injected`                                    | –     |
| `μ_counts/mm`   | `-ln(T_counts) / thickness_mm`                                 | 1/mm  |
| `(μ/ρ)_counts`  | `μ_counts/mm × 10 / density_g_cm3`                             | cm²/g |
| `(μ_en/ρ)_raw`  | `(E_dep / (E0 × N_injected)) / (ρ × thickness_cm)`             | cm²/g |
| `(μ_en/ρ)_CPE`  | `(μ/ρ)_counts × (μ_en/μ)_NIST` (또는 참조가 없으면 `μ_tr/ρ`) | cm²/g |
| `μ_eff/mm`      | `-ln(1 - absorbedFraction) / thickness_mm`                     | 1/mm  |

### 매크로 워크플로
- `mac/init.mac` : 세계/박막/백킹 설정과 시드 고정
- `mac/e.mac` : 단일 에너지 런 (현 alias 사용)
- `mac/gamma.mac` : 356.5 keV, 100 k 프라이머리 스모크 테스트
- `mac/benchmark.mac` : 에너지별로 박막 두께를 달리(60 keV 200 nm, 80 keV 300 nm, 500 keV 250 µm, 1000 keV 750 µm)해 고광학두께 조건 보장
- `mac/benchmark_core.mac` : 백킹 두께를 `/control/alias backingThickness_um <값>`으로 지정한 뒤 실행하는 공통 벤치마크 루틴
- `mac/benchmark_compare.mac` : 백킹을 끈 상태(0 µm)와 기본 50 µm 백킹을 연속으로 실행해 동일 CSV에 두 결과를 기록
- `mac/energy_full.mac` : 1 keV–367.7 keV 로그 스캔
- `mac/energy_high.mac` : 500 keV–100 MeV 확장 스캔
- `mac/energy_nist.mac` : 참조 CSV에 있는 50개 에너지를 그대로 실행
- `mac/thickness_scan.mac` : 세계(50/100 cm) × 박막(100, 150, 250, 500 nm, 1, 1.5, 2, 5, 10 µm) × 에너지 전체 스윕, 백킹을 필요에 따라 켜고 끔. 얇은 박막은 저에너지 포토전 효과를, 두꺼운 박막은 고에너지에서의 콤프턴·쌍생성 감쇠를 안정적으로 측정하기 위함입니다.
- `mac/nist_scan.mac` : 동일한 세계/박막 조합에서 NIST 에너지 목록만 실행해 참조 표와 1:1 비교

### 출력 파일과 CSV 열 설명
`transmission_summary.csv`는 기본 라인과 동일한 스키마를 사용합니다.
- 지오메트리/재질: `run_id`, `world_half_cm`, `thickness_nm`, `density_g_cm3`, `E_keV`
- 백킹 정보: `backing_thickness_um`
- 계수 데이터:
  - `mu_counts_per_mm`, `mu_counts_cm2_g`
  - `mu_calc_per_mm`, `mu_calc_cm2_g`
  - `mu_tr_per_mm`, `mu_tr_cm2_g`
  - `mu_en_cpe_per_mm`, `mu_en_cpe_cm2_g`
  - `mu_en_per_mm`, `mu_en_cm2_g` (CPE 보정), `mu_en_raw_per_mm`, `mu_en_raw_cm2_g`
  - `mu_eff_per_mm`, `mu_eff_cm2_g`
- 참조 및 오차:
  - `mu_ref_cm2_g`, `mu_en_ref_cm2_g`
  - `delta_mu_percent`, `delta_mu_en_percent`, `delta_mu_en_cpe_percent`
- 통계 및 에너지:
  - `N_injected`, `N_uncollided`, `N_trans_total`, `T_counts`
  - `sigma_T_counts`, `sigma_mu_counts_cm2_g`
  - `E_trans_unc_keV`, `E_trans_tot_keV`, `E_abs_keV`, `T_energy_tot`
- 기타: `absorbed_fraction`은 포일(및 백킹)이 흡수한 에너지 비율을 의미합니다.

### 분석 절차
- `plot_coefficients.py --csv transmission_summary.csv --reference nist_reference.csv --output-dir plots_variants`
  - 두께별 로그-로그 그래프를 생성하며, 그래프 상단에는 시뮬레이션 곡선과 NIST 곡선/포인트가 함께 표시되고 하단에는 백분율 잔차 그래프가 추가됩니다.
  - 참조 파일을 제공하면 `plots_variants/errors/`에 오차 표를 저장합니다.
- `generate_nist_error.py`는 최신 런과 참조 값을 결합한 표를 제공합니다.

### 권장 실행 순서
```bash
rm -f transmission_summary.csv
./build/attenuation mac/benchmark_compare.mac
./build/attenuation mac/nist_scan.mac
python plot_coefficients.py --csv transmission_summary.csv --reference nist_reference.csv --output-dir plots_variants
```
이 과정을 통해 백킹을 포함한 구성이든, 백킹을 제거한 구성이든 동일한 NIST 에너지 집합에서 시뮬레이션 값을 추출하고 오차 테이블을 생성할 수 있습니다. 필요하면 `mac/thickness_scan.mac`을 사용해 전체 에너지 스펙트럼을 반복한 뒤 동일한 플롯 스크립트를 돌려 비교합니다.

### basic 빌드와의 차이
- variants는 `/det/setBackingThickness` 명령을 통해 Foil 뒤에 백킹을 둘 수 있으며, `SteppingAction`이 Foil→Backing→World 경계 각각에서 한 번만 투과를 기록합니다.
- `TrackInfo`에 `TransmissionLogged` 플래그를 추가해 다중 영역을 통과하는 포톤을 중복 집계하지 않습니다.
- 나머지 물리 리스트, 난수 시드, CSV 스키마, 플로팅 파이프라인은 기본 라인과 동일합니다.

---

## English Version

### Overview
The variants build extends the basic benchmark by exposing `/det/setBackingThickness` so I can insert a downstream tungsten backing slab. The stepping logic tracks transitions across Foil→Backing and Backing→World boundaries and records each photon’s transmission exactly once. Random seeds remain fixed at `/random/setSeeds 123456 789012`, keeping runs reproducible.

### Physics model and equations
1. **Transmission channel**
   ```
   T_counts     = N_uncollided / N_injected
   μ_counts/mm  = -ln(T_counts) / thickness_mm
   (μ/ρ)_counts = (μ_counts/mm × 10) / density_g_cm3
   ```
2. **Energy absorption**
   ```
   (μ_en/ρ)_raw = (E_dep / (E0 × N_injected)) / (ρ × thickness_cm)
   (μ_en/ρ)_CPE = (μ/ρ)_counts × (μ_en/μ)_NIST   [or μ_tr/ρ]
   ```
3. **Effective attenuation**
   ```
   μ_eff/mm = -ln(1 - absorbedFraction) / thickness_mm
   μ_eff/ρ  = (μ_eff/mm × 10) / ρ
   ```
4. **Diagnostics**
   - `μ_calc` and `μ_tr` continue to come from `G4EmCalculator` plus the Compton retention integral. Every row keeps `backing_thickness_um` so you can correlate deviations with the downstream slab thickness.

| Quantity        | Definition / Notes                                             | Units |
|-----------------|----------------------------------------------------------------|-------|
| `T_counts`      | `N_uncollided / N_injected`                                    | –     |
| `μ_counts/mm`   | `-ln(T_counts) / thickness_mm`                                 | 1/mm  |
| `(μ/ρ)_counts`  | `μ_counts/mm × 10 / density_g_cm3`                             | cm²/g |
| `(μ_en/ρ)_raw`  | `(E_dep / (E0 × N_injected)) / (ρ × thickness_cm)`             | cm²/g |
| `(μ_en/ρ)_CPE`  | `(μ/ρ)_counts × (μ_en/μ)_NIST` (fallback to `μ_tr/ρ` if needed) | cm²/g |
| `μ_eff/mm`      | `-ln(1 - absorbedFraction) / thickness_mm`                     | 1/mm  |

### Macro workflow
- `mac/init.mac` – configure world, foil, backing, and seeds.
- `mac/e.mac` – single-energy helper.
- `mac/gamma.mac` – 356.5 keV smoke test (100 k primaries).
- `mac/benchmark.mac` – adaptive foil thicknesses per energy (200 nm at 60 keV up to 750 µm at 1 MeV).
- `mac/benchmark_core.mac` – core routine; define `/control/alias backingThickness_um <value>` (µm) before execution to choose backing thickness.
- `mac/benchmark_compare.mac` – runs `benchmark_core.mac` twice (0 µm and 50 µm backing) so foil-only and backed results land in the same CSV.
- `mac/energy_full.mac` – 1–367.7 keV log-spaced sweep.
- `mac/energy_high.mac` – 0.5–100 MeV sweep.
- `mac/energy_nist.mac` – replay the exact 50 energies from `nist_reference.csv`.
- `mac/thickness_scan.mac` – world half-lengths (50, 100 cm) × thicknesses (100, 150, 250, 500 nm; 1, 1.5, 2, 5, 10 µm) × full energy grids, with optional backing toggles. Thin foils probe photoelectric-heavy energies, while thick/µm foils keep high-energy runs away from the `T≈1` regime so μ extraction remains stable.
- `mac/nist_scan.mac` – same geometry grid but restricted to the NIST energy list for one-to-one comparisons.

### Output files and column glossary
`transmission_summary.csv` mirrors the basic schema:
- Geometry & material: `run_id`, `world_half_cm`, `thickness_nm`, `density_g_cm3`, `E_keV`
- Backing metadata: `backing_thickness_um`
- Transmission & energy:
  - `N_injected`, `N_uncollided`, `N_trans_total`, `T_counts`
  - `mu_counts_per_mm`, `mu_counts_cm2_g`
  - `mu_en_raw_per_mm`, `mu_en_raw_cm2_g`
  - `mu_en_cpe_per_mm`, `mu_en_cpe_cm2_g`, `mu_en_per_mm`, `mu_en_cm2_g`
  - `mu_eff_per_mm`, `mu_eff_cm2_g`
- Calculator & reference:
  - `mu_calc_per_mm`, `mu_calc_cm2_g`, `mu_tr_per_mm`, `mu_tr_cm2_g`
  - `mu_ref_cm2_g`, `mu_en_ref_cm2_g`
  - `delta_mu_percent`, `delta_mu_en_percent`, `delta_mu_en_cpe_percent`
- Statistics & energy balance:
  - `sigma_T_counts`, `sigma_mu_counts_cm2_g`
  - `absorbed_fraction`
  - `E_trans_unc_keV`, `E_trans_tot_keV`, `E_abs_keV`, `T_energy_tot`

### Analysis tools
- `plot_coefficients.py --csv transmission_summary.csv --reference nist_reference.csv --output-dir plots_variants`
  - Produces per-thickness log–log plots with both the simulation curves and NIST curves/points, plus a residual subplot that charts the percentage differences.
  - Writes detailed error tables to `plots_variants/errors/` when a reference CSV is provided, reporting absolute and percentage deviations for μ/ρ, μ_en(CPE), and μ_en(raw).
- `generate_nist_error.py` – compresses the latest simulation row per energy into a comparison table.

Recommended workflow:
```bash
rm -f transmission_summary.csv
./build/attenuation mac/benchmark_compare.mac
./build/attenuation mac/nist_scan.mac
python plot_coefficients.py --csv transmission_summary.csv --reference nist_reference.csv --output-dir plots_variants
```
This produces a reproducible data set aligned with the NIST energy list plus error tables that are ready for slide decks or reports.

### Relationship to the basic line
- Variants supports a downstream backing slab and enhanced stepping, while the basic executable keeps a free-standing foil.
- Both share the HybridEmPhysics list, deterministic seeding, CSV schema, and plotting pipeline so that differences in output stem only from the intentional geometry/stepping changes.
