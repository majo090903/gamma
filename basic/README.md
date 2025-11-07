이 문서는 한국어와 영어 두 가지 버전으로 제공되며, 아래에 한국어 설명이 먼저 나옵니다.

---

## 한국어 버전

### 개요
이 기본(basic) 빌드는 텅스텐 박막을 통과하는 감마선 감쇠를 검증하기 위한 가장 단순한 구성입니다. `HybridEmPhysics`(= `G4EmStandardPhysics_option4` 기반) 물리 리스트를 사용하고, 박막 뒤에 추가 물질을 두지 않은 순수한 투과 지오메트리로 설정되어 있습니다. 모든 매크로는 `/random/setSeeds 123456 789012`를 통해 동일한 난수 시드를 사용하므로, 동일한 매크로를 반복 실행하면 결과 CSV가 항상 동일하게 재현됩니다.

### 물리 계산과 사용된 식
계산 파이프라인을 단계별로 정리하면 다음과 같습니다.

1. **투과 기반 μ/ρ**
   ```
   T_counts      = N_uncollided / N_injected
   μ_counts/mm   = -ln(T_counts) / thickness_mm
   (μ/ρ)_counts  = (μ_counts/mm × 10) / density_g_cm3
   ```
   `σ_T_counts`와 `σ_(μ/ρ)`는 동일식을 이용해 전파됩니다.

2. **에너지 흡수 계수**
   ```
   μ_en,raw/ρ = (E_dep / (E0 × N_injected)) / (ρ × thickness_cm)
   μ_en,raw/mm = μ_en,raw/ρ × ρ / 10
   ```
   원시(raw) 값은 얇은 박막에서 전하입자 평형이 깨지므로 NIST보다 크게 낮습니다.

3. **CPE 보정**
   ```
   ratio_ref      = (μ_en/ρ)_ref / (μ/ρ)_ref
   (μ_en/ρ)_CPE   = (μ/ρ)_counts × ratio_ref   (참조 없으면 μ_tr/ρ 사용)
   μ_en,CPE/mm     = μ_en,CPE/ρ × ρ / 10
   ```
   이렇게 얻은 `(μ_en/ρ)_CPE`가 CSV 및 로그의 기본 보고 값입니다.

4. **유효 감쇠 계수**
   ```
   μ_eff/mm  = -ln(1 - absorbedFraction) / thickness_mm
   μ_eff/ρ   = (μ_eff/mm × 10) / ρ
   ```

5. **계산기 비교 및 보조량**
   - `G4EmCalculator`의 포토, 콤프턴, 레이리, 쌍생성 단면 합으로 `μ_calc/mm`을 구성하고, 질량 변화로 `μ_calc/ρ`를 계산합니다.
   - 콤프턴 잔류 에너지를 512점 심프슨 적분으로 구해 `μ_tr` (transfer coefficient)를 얻습니다. 참조 데이터가 없을 때 `(μ_en/ρ)_CPE`는 이 `μ_tr/ρ`로 교체됩니다.
   - 로그에는 `[diag]` 블록으로 `(μ/ρ)_counts` vs `μ_calc/ρ`, `(μ_en/ρ)_CPE` vs `μ_tr/ρ` 비교가 함께 출력되어 디버깅에 도움을 줍니다.

| Quantity        | Definition / Notes                                             | Units |
|-----------------|----------------------------------------------------------------|-------|
| `T_counts`      | `N_uncollided / N_injected`                                    | –     |
| `μ_counts/mm`   | `-ln(T_counts) / thickness_mm`                                 | 1/mm  |
| `(μ/ρ)_counts`  | `μ_counts/mm × 10 / density_g_cm3`                             | cm²/g |
| `(μ_en/ρ)_raw`  | `(E_dep / (E0 × N_injected)) / (ρ × thickness_cm)`             | cm²/g |
| `(μ_en/ρ)_CPE`  | `(μ/ρ)_counts × (μ_en/μ)_NIST` (또는 참조가 없으면 `μ_tr/ρ`) | cm²/g |
| `μ_eff/mm`      | `-ln(1 - absorbedFraction) / thickness_mm`                     | 1/mm  |

### 매크로 워크플로
- `mac/init.mac` : 지오메트리/빔 초기화와 시드 고정
- `mac/e.mac` : 단일 에너지 런(에너지와 입사수는 `/control/alias`로 지정)
- `mac/gamma.mac` : 356.5 keV, 10만 프라이머리 스모크 테스트
- `mac/benchmark.mac` : 60/80/500/1000 keV에서 25 nm 박막을 반복 실행
- `mac/energy_full.mac` : 1 keV–367.7 keV 로그 간격 스캔
- `mac/energy_high.mac` : 500 keV–100 MeV 확장 스캔
- `mac/energy_nist.mac` : `nist_reference.csv`에 포함된 50개 에너지를 그대로 실행
- `mac/thickness_scan.mac` : 세계 크기 50·100 cm, 박막 100 nm–10 µm 범위(100, 150, 250, 500 nm, 1, 1.5, 2, 5, 10 µm)로 전체 에너지 스캔. 얇은 두께는 저에너지(포토전효과 지배) 구간을, 두꺼운 두께는 고에너지(콤프턴·쌍생성) 구간을 안정적으로 샘플링하기 위함입니다.
- `mac/nist_scan.mac` : 동일한 두께/세계 조합에 대해 NIST 에너지 목록만 실행해 참조 표와 1:1 비교

### 출력 파일과 CSV 열 설명
모든 런은 `transmission_summary.csv`에 행을 추가합니다. 주요 열은 다음과 같습니다.
- `run_id` : Geant4 런 인덱스
- `world_half_cm`, `thickness_nm`, `density_g_cm3` : 지오메트리 파라미터
- `E_keV` : 평균 입사 에너지
- `N_injected`, `N_uncollided`, `N_trans_total` : 입사/비산란/전체 투과 개수
- `T_counts` : 비산란 투과율
- `mu_counts_per_mm`, `mu_counts_cm2_g` : 로그 기반 감쇠 계수(길이 기준, 질량 기준)
- `mu_calc_per_mm`, `mu_calc_cm2_g` : Geant4 계산기 추정치
- `mu_tr_per_mm`, `mu_tr_cm2_g` : 에너지 전달 계수(콤프턴 잔류 보정 포함)
- `mu_ref_cm2_g`, `mu_en_ref_cm2_g` : 참조 표(있을 때)에서 선형 보간된 μ/ρ, μ_en/ρ
- `delta_mu_percent`, `delta_mu_en_percent` : 시뮬레이션 vs 참조 백분율 오차(μ/ρ, μ_en raw)
- `sigma_T_counts`, `sigma_mu_counts_cm2_g` : 투과율과 μ/ρ에 대한 통계적 표준편차
- `mu_en_cpe_per_mm`, `mu_en_cpe_cm2_g` : CPE 보정된 μ_en/ρ (길이/질량)
- `delta_mu_en_cpe_percent` : CPE 보정치와 참조 간 백분율 오차
- `mu_en_per_mm`, `mu_en_cm2_g` : 현재 보고에 사용되는 μ_en/ρ (CPE 보정)
- `mu_en_raw_per_mm`, `mu_en_raw_cm2_g` : 원시 에너지 감쇠 계수
- `mu_eff_per_mm`, `mu_eff_cm2_g` : 유효 감쇠 계수
- `E_trans_unc_keV`, `E_trans_tot_keV`, `E_abs_keV` : 비산란 투과 에너지, 총 투과 에너지, 박막 내 흡수 에너지
- `T_energy_tot` : 총 에너지 투과율

### 분석과 비교
- `plot_coefficients.py --csv transmission_summary.csv --reference nist_reference.csv`
  - 두께별 로그-로그 그래프를 생성하며, 그래프 상단에는 시뮬레이션과 NIST 곡선/포인트가 함께 표시되고 하단에는 백분율 잔차 그래프가 추가됩니다 (`plots/`).
  - 참조 데이터를 제공하면 `plots/errors/` 폴더에 두께별 오차 표(`errors_<thickness>nm.csv`)를 추가합니다.
    - 열 구성: μ/ρ, μ_en(raw), μ_en(CPE)의 절대/백분율 오차와 참조 값.
- `generate_nist_error.py` : 특정 에너지당 최신 결과와 참조를 매칭하는 별도 비교 표 생성.

### 활용 팁
```bash
rm -f transmission_summary.csv
./build/attenuation mac/nist_scan.mac
python plot_coefficients.py --csv transmission_summary.csv --reference nist_reference.csv
```
위 명령으로 NIST 에너지만 포함하는 CSV와 오차 테이블을 만들 수 있습니다. 필요하면 `mac/thickness_scan.mac`으로 전체 에너지 스캔을 수행한 뒤 같은 플롯 스크립트를 돌려 전체 곡선을 확인합니다.

### variants 빌드와의 차이점
- 기본 빌드는 박막 뒤의 백킹(backing) 물질이 없고, Foil→World 경계에서만 투과를 기록합니다.
- variants 빌드는 백킹 두께를 UI 명령으로 조절하며, Foil→Backing→World 경계를 각각 추적합니다.
- 두 빌드는 동일한 물리 리스트, 난수 시드, CSV 스키마, 플롯 파이프라인을 공유합니다.

---

## English Version

### Overview
The basic line is my minimal tungsten attenuation benchmark. It places a single self-supporting foil in vacuum, drives a monoenergetic gamma pencil beam, and uses the HybridEmPhysics (Option4) list to cover photoelectric through pair production processes. Every macro fixes the random engine to `/random/setSeeds 123456 789012`, so rerunning a macro reproduces the same CSV rows byte-for-byte.

### Physics model and equations
1. **Counts-based channel**
   ```
   T_counts     = N_uncollided / N_injected
   μ_counts/mm  = -ln(T_counts) / thickness_mm
   (μ/ρ)_counts = (μ_counts/mm × 10) / density_g_cm3
   ```
2. **Energy channel**
   ```
   (μ_en/ρ)_raw = (E_dep / (E0 × N_injected)) / (ρ × thickness_cm)
   (μ_en/ρ)_CPE = (μ/ρ)_counts × (μ_en/μ)_NIST     [or μ_tr/ρ if no reference]
   ```
3. **Effective attenuation**
   ```
   μ_eff/mm = -ln(1 - absorbedFraction) / thickness_mm
   μ_eff/ρ  = (μ_eff/mm × 10) / ρ
   ```
4. **Diagnostics**
   - `G4EmCalculator` accumulates the relevant cross sections to deliver `μ_calc` and, together with the Compton retention integral, the transfer coefficient `μ_tr`.
   - All deltas are reported as percentages (`Δ μ/ρ`, `Δ μ_en/raw`, `Δ μ_en/CPE`) and stored in `transmission_summary.csv`.

| Quantity        | Definition / Notes                                             | Units |
|-----------------|----------------------------------------------------------------|-------|
| `T_counts`      | `N_uncollided / N_injected`                                    | –     |
| `μ_counts/mm`   | `-ln(T_counts) / thickness_mm`                                 | 1/mm  |
| `(μ/ρ)_counts`  | `μ_counts/mm × 10 / density_g_cm3`                             | cm²/g |
| `(μ_en/ρ)_raw`  | `(E_dep / (E0 × N_injected)) / (ρ × thickness_cm)`             | cm²/g |
| `(μ_en/ρ)_CPE`  | `(μ/ρ)_counts × (μ_en/μ)_NIST` (fallback to `μ_tr/ρ` if needed) | cm²/g |
| `μ_eff/mm`      | `-ln(1 - absorbedFraction) / thickness_mm`                     | 1/mm  |

### Macro workflow
- `mac/init.mac` – prepare geometry/source seeds.
- `mac/e.mac` – run a single energy with current aliases.
- `mac/gamma.mac` – smoke test at 356.5 keV, 100 k primaries.
- `mac/benchmark.mac` – 60/80/500/1000 keV at 250 nm.
- `mac/energy_full.mac` – 70 log-spaced energies (1–367.7 keV).
- `mac/energy_high.mac` – high-energy sweep (0.5–100 MeV).
- `mac/energy_nist.mac` – replay the 50 energies listed in `nist_reference.csv`.
- `mac/thickness_scan.mac` – sweep all energies over foil thicknesses (100, 150, 250, 500 nm; 1, 1.5, 2, 5, 10 µm) and world half-lengths (50, 100 cm). Thin foils capture the photoelectric-dominated regime, while thicker foils keep high-energy runs away from the `T→1` limit so the logarithmic inversion stays stable.
- `mac/nist_scan.mac` – same geometry grid but restricted to the NIST energies for direct table comparisons.

### Output files and CSV columns
Every run appends a row to `transmission_summary.csv`. Key columns include:
- `run_id`, `world_half_cm`, `thickness_nm`, `density_g_cm3`, `E_keV`
- `N_injected`, `N_uncollided`, `N_trans_total`, `T_counts`
- `mu_counts_per_mm`, `mu_counts_cm2_g`
- `mu_calc_per_mm`, `mu_calc_cm2_g`, `mu_tr_per_mm`, `mu_tr_cm2_g`
- `mu_ref_cm2_g`, `mu_en_ref_cm2_g`
- `delta_mu_percent`, `delta_mu_en_percent`
- `sigma_T_counts`, `sigma_mu_counts_cm2_g`
- `mu_en_cpe_per_mm`, `mu_en_cpe_cm2_g`, `delta_mu_en_cpe_percent`
- `mu_en_per_mm`, `mu_en_cm2_g` (CPE-adjusted), `mu_en_raw_per_mm`, `mu_en_raw_cm2_g`
- `mu_eff_per_mm`, `mu_eff_cm2_g`
- `E_trans_unc_keV`, `E_trans_tot_keV`, `E_abs_keV`, `T_energy_tot`

### Analysis tools
- `plot_coefficients.py --csv transmission_summary.csv --reference nist_reference.csv`
  - Generates log–log overlays per thickness in `plots/`, plotting the simulation curves alongside the NIST curves/points and a residual subplot with percentage differences.
  - When a reference file is provided, the script also writes per-thickness error tables in `plots/errors/`, reporting absolute and percentage deviations for μ/ρ, μ_en(CPE), and μ_en(raw).
- `generate_nist_error.py` – produces a condensed table that keeps only the latest simulation row per energy.

Suggested workflow:
```bash
rm -f transmission_summary.csv
./build/attenuation mac/nist_scan.mac
python plot_coefficients.py --csv transmission_summary.csv --reference nist_reference.csv
```
This produces a data set aligned with the NIST energies plus detailed error tables for presentations or reports.

### Relation to the variants line
- The basic executable uses a free-standing foil with no downstream backing volume.
- The variants line adds `/det/setBackingThickness`, enhanced stepping, and tailored benchmark thicknesses for high energies.
- Both share the same physics list, RNG discipline, CSV schema, and plotting pipeline, so outputs remain directly comparable.
