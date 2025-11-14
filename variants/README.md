이 문서는 한국어와 영어 두 가지 버전으로 제공되며, 아래에 한국어 설명이 먼저 나옵니다.

---

## 한국어 버전

### 폴더 구조 요약
```
variants/
├── mac/
│   ├── thickness_scan.mac        <-- 2×9×(46/18) 에너지 러닝을 수행
│   ├── benchmark_compare.mac     <-- Foil-only vs 백킹 비교 (4 에너지 × 2 백킹)
│   └── benchmark_auto.mac        <-- 최적 조합 재실행용 자동 생성 매크로
├── transmission_summary.csv      <-- 모든 러닝 결과를 누적한 마스터 CSV
├── plots_variants/
│   ├── foil_only/                <-- 백킹 0 µm 조합 PNG
│   ├── backed/                   <-- 백킹 50 µm 조합 PNG
│   ├── best/                     <-- 최고 성능 플롯/오차 CSV (rank_thickness_accuracy.py)
│   └── errors/
│       ├── foil_only/            <-- Foil-only 오차 데이터
│       └── backed/               <-- 백킹 포함 오차 데이터
├── overlay_best_thickness.{png,csv}   <-- 에너지별 최적 조합 오버레이
├── optimized_thicknesses.csv     <-- 오버레이를 표로 정리
├── best_thickness_configs.csv    <-- RMS 기준 foil-only & 백킹 우승자
├── thickness_accuracy_rankings.csv / coverage_report.csv  <-- 분석 스크립트 출력
└── scripts (plot_coefficients.py, overlay_best_thickness.py, validate_summary.py, …)
```
#### 발표용 추천 자료
- `overlay_best_thickness.png` & `overlay_best_thickness.csv` <== 발표 추천: 에너지별로 어떤 두께/백킹이 NIST에 가장 근접했는지 한 장에 정리되어 있어 슬라이드 첫 장에 쓰기 좋습니다.
- `plots_variants/best/` <== 발표 추천: `rank_thickness_accuracy.py`가 선정한 Foil-only/백킹 최고 조합의 PNG·오차 CSV가 모여 있어, 비교 그래프를 바로 붙여넣을 수 있습니다.
- `best_thickness_configs.csv` + `optimized_thicknesses.csv` <== 발표 추천: “어떤 조합이 가장 정확했는가”를 수치로 설명할 때 사용합니다.
- `coverage_report.csv`와 `thickness_accuracy_rankings.csv`: 러닝이 얼마나 광범위했는지, 평균적으로 어느 조합이 가장 NIST에 가까웠는지 부록 슬라이드로 넣을 수 있습니다.

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

#### CPE와 에너지 흡수 계수
- **`(μ_en/ρ)_raw (foil)`** : 포일 내부에만 실제로 침적된 에너지로부터 계산한 값입니다. 얇은 박막에서는 2차 전자가 쉽게 탈출하기 때문에 NIST 자료에 비해 항상 작습니다.
- **`(μ_en/ρ)_raw (slab)`** : 포일과 백킹이 흡수한 에너지를 모두 포함하고 전체 질량 경로(포일 + 백킹 밀도)를 사용합니다. 백킹이 충분히 두꺼워지면 CPE 조건을 점점 더 잘 근사합니다.
- **`(μ_en/ρ)_CPE`** : 전통적인 NIST 정의를 모사하기 위해 `μ_counts`에 `(μ_en/μ)_NIST` 비율을 곱하거나, 참조가 없을 때는 `μ_tr`에 기반한 값을 사용합니다.

이제 `plot_coefficients.py`에 `--show-raw-foil`, `--show-raw-slab`을 주면 세 가지 계수를 한눈에 비교할 수 있습니다. 백킹 두께나 재질을 조절해 슬랩 값이 NIST에 얼마나 빠르게 수렴하는지도 확인할 수 있습니다.

#### 추가 진단과 제어
- **새로운 UI 명령**: `/det/setBackingMaterial <G4 재질 이름>`으로 백킹을 텅스텐 이외의 재질로 교체할 수 있습니다. 기본값은 `G4_W`이며, 백킹을 끈 상태(0 µm)에서도 재질 설정이 보존됩니다.
- **참조 인터폴레이션**: 환경 변수 `W_USE_LOG_INTERP=1`을 설정하면 `InterpolateReference`가 log-log 공간에서 NIST 표를 보간합니다. 기본값은 선형 보간입니다.
- **μ_tr 제어**: `W_USE_MU_TR_CALC=0`으로 설정하면 참조가 없을 때 `μ_en/ρ`를 `μ_tr`로 대체하지 않고 원시 에너지 흡수 결과를 그대로 유지할 수 있습니다.
- **콤프턴 적분 해상도**: `W_COMPTON_STEPS=<짝수>`로 심프슨 적분 스텝 수를 조정할 수 있습니다(기본 512). 저에너지에서 `μ_tr`의 통계적 잡음을 줄이는 데 사용합니다.
- **전송 클램프**: `T_counts_clamped`, `clamp_flag` 열이 추가되어 T가 0 또는 1에 붙을 때 로그 계산이 안전하게 클램프되었는지를 확인할 수 있습니다. `clamp_flag=0/1/2`는 클램프 여부(하한/상한)를 나타냅니다.
- **산란 분리**: `N_scattered`, `T_counts_scattered` 열이 산란 전달을 따로 기록해 `T_counts`와 비교할 수 있습니다.
- **에너지 분해**: `E_abs_backing_keV`, `E_abs_slab_keV`, `E_abs_other_keV`, `absorbed_fraction_slab`이 추가되어 포일, 백킹, 그 외 영역의 에너지 저장 비율을 분리해 확인할 수 있습니다.
- **원시 μ_en 열 분리**: `mu_en_raw_cm2_g`/`mu_en_raw_per_mm`는 포일만을 의미하고, `mu_en_raw_slab_cm2_g`/`mu_en_raw_slab_per_mm`는 포일+백킹을 의미합니다.
- **새 잔차 진단**: `delta_mu_counts_vs_mu_calc_percent`, `delta_mu_en_cpe_vs_mu_tr_percent`를 통해 `μ_calc`, `μ_tr`와의 편차를 빠르게 확인할 수 있습니다.

#### Foil-only vs Backed
- **Foil-only**는 `/det/setBackingThickness 0 um` 상태로 실행된 러닝 집합입니다. 얇은 박막에서 충전입자 평형이 깨졌을 때 `(μ_en/ρ)_raw`가 얼마나 축소되는지, μ/ρ 추정이 NIST와 얼마나 달라지는지 확인하는 용도로 사용합니다. 모든 결과는 `plots_variants/foil_only/`와 `plots_variants/errors/foil_only/`에 정리됩니다.
- **Backed**는 기본 백킹(50 µm W) 또는 `/det/setBackingMaterial`로 지정한 재질을 장착한 상태입니다. Foil+백킹의 질량 경로로 에너지를 나눈 `(μ_en/ρ)_raw(slab)`와 CPE 보정값 `(μ_en/ρ)_CPE`는 NIST 표에 훨씬 근접하며, 해당 플롯/CSV는 `plots_variants/backed/`·`plots_variants/errors/backed/`에서 찾을 수 있습니다.
- 두 구성의 데이터가 동일한 에너지 격자를 공유하도록 `thickness_scan.mac`과 `thickness_scan_foil.mac`을 따로 돌려 두 세트를 구축했으므로, PPT에서 “백킹 유무 비교” 슬라이드를 만들 때 데이터를 그대로 사용할 수 있습니다.

#### 백킹(backing)이란?
- Foil 뒤에 놓인 고밀도 슬랩(기본값 50 µm 텅스텐)으로, 포일에서 방출된 2차 전자가 곧바로 진공으로 탈출하지 않고 충분한 두께 내에서 모두 멈추도록 도와줍니다.
- 이는 NIST가 정의한 “무한한 균질 매질에서의 Charged-Particle Equilibrium”을 강제로 회복시켜 `(μ_en/ρ)_raw (slab)`이 `(μ_en/ρ)_NIST`에 접근하게 만듭니다.
- `/det/setBackingMaterial`을 이용해 Al, Cu 등 다른 재질을 선택하면 전자 제동 복잡도를 비교할 수 있으며, `overlay_best_thickness.py` 결과에서 마커 모양(원: 백킹 없음, 사각형: 백킹 있음)으로 두 경우를 시각적으로 구분합니다.

#### 지오메트리와 빔 구성 이유
- `mac/init.mac`은 항상 `/random/setSeeds 123456 789012`를 호출해 모든 매크로가 동일한 난수열에서 시작하도록 합니다. 이렇게 해야 foil-only와 backed 구성을 라인별로 뺄셈했을 때 통계적 잡음이 최소화됩니다.
- GPS는 반지름 0.5 mm의 원판을 Foil 앞 25 cm에 놓고 +z 방향으로만 쏘도록 구성되어 있습니다. 이는 NIST의 “collimated narrow beam” 조건을 그대로 구현하기 위한 것으로, 광선이 Foil 앞면을 균일하게 덮으면서도 거의 평행하게 입사합니다.
- World 반길이(half-length)를 50 cm 또는 100 cm로 잡으면, Foil에서 튕겨 나온 전자나 포톤이 월드 경계에 닿기 전에 충분히 흡수되어 다시 Foil로 돌아오는 일이 드뭅니다. World가 너무 작으면 재입사(backscatter)가 빈번해져 μ/ρ 비교가 왜곡됩니다.
- `SteppingAction`은 Foil→Backing, Backing→World, Foil→World 경계를 모두 감시해 +z 방향으로 나가는 순간에만 `RecordTransmission`을 호출하고, 곧바로 트랙을 kill해 다시 Foil로 돌아오는 일을 원천 차단합니다.

#### 두께·백킹 값이 이렇게 정해진 이유
- `mac/benchmark_core.mac`의 60 keV 200 nm, 80 keV 300 nm, 500 keV 250 µm, 1000 keV 750 µm 조합은 모두 광학 깊이 τ = μρt가 0.3~3 사이에 위치하도록 손으로 맞춘 값입니다. τ가 0.01 이하이면 `T_counts ≈ 1`이 되어 로그 계산이 잡음에 민감하고, τ가 5 이상이면 `T_counts ≈ 0`이 되어 유효한 μ를 추정하기 힘듭니다.
- 기본 백킹 두께 50 µm 텅스텐은 100 keV~1 MeV 범위에서 대부분의 2차 전자가 그 안에서 멈출 수 있는 두께입니다. 필요하면 `/det/setBackingThickness`로 더 얇거나 두꺼운 슬랩을 시험하고, `overlay_best_thickness.py`가 에너지별로 어떤 값이 최적인지 리스트업해 줍니다.
- `mac/thickness_scan.mac`과 `mac/nist_scan.mac`은 세계 길이(50/100 cm) × 두께(100 nm–10 µm)를 전부 훑으면서 각 에너지에서 T가 적당한지 확인하는 용도입니다. 얇은 박막은 낮은 에너지에서 포토전 주도 구간을, 두꺼운 박막은 높은 에너지에서 콤프턴/쌍생성 감쇠를 안정적으로 측정하기 위해 사용합니다.
- **물리적 해석**:
  - 저에너지(≲100 keV)에서는 포토전 효과가 우세하여 한 번 상호작용하면 거의 모든 에너지를 포일이 흡수합니다. 이때 너무 두꺼운 포일을 쓰면 포톤이 대부분 첫 수 µm 안에서 사라져 통계적으로 의미 있는 전송 샘플이 줄어들므로 100–300 nm급 박막을 선택합니다.
  - 중에너지(100 keV–1 MeV)에서는 콤프턴 산란이 지배적이며 산란된 포톤이 다시 Foil을 통과할 수 있으므로 포일을 수백 nm~수백 µm로 두껍게 만들어 `T_counts`를 안정화합니다. 동시에 2차 전자의 유효 범위가 수십 µm로 커지기 때문에 백킹을 두어 에너지 평형을 맞춥니다.
  - 고에너지(>1 MeV)에서는 쌍생성까지 등장해 전하 입자 범위가 더 길어지고 μ/ρ가 작아지므로 최소 250 µm~1 mm 두께가 필요합니다. 이 영역에서 백킹을 알루미늄·구리 등 다른 재질로 바꾸면 전자 정지능 차이를 직접 비교할 수 있습니다.
  - `overlay_best_thickness.py`와 `optimize_thickness.py`는 이런 물리적 경향을 수치화해 에너지별 “최적” 두께/백킹/월드 조합을 제시합니다. `overlay_best_thickness.png`에는 NIST 곡선과 최적 포인트가 동시에 표시되고, `mac/benchmark_auto.mac`은 그 조합을 재실행하는 매크로입니다.

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

#### 러닝 통계 (모두 Geant4 11.3.2, HybridEmPhysics, 200k 프라이머리 기준)
- `thickness_scan.mac` : 세계 반길이 50 cm & 100 cm 각각에 대해 (100, 150, 250, 500 nm, 1, 1.5, 2, 5 µm) × `energy_full.mac`(46 에너지) + (10 µm × `energy_high.mac` 18 에너지) = 2 × (8×46 + 1×18) = **772** 런. 모든 런은 백킹 50 µm를 포함합니다.
- `thickness_scan_foil.mac` : 동일한 세계/두께/에너지 조합을 백킹 0 µm 상태에서 반복해 Foil-only 데이터 세트를 구축합니다(**772** 런).
- `benchmark_compare.mac` : 60, 80, 500, 1000 keV에서 포일만/백킹 50 µm 조합을 연속 실행(**8** 런)해 동일 난수로 Foil-only 데이터를 확보합니다.
- `nist_scan.mac` : 세계 50 cm/100 cm × 두께 10종(100 nm–10 µm) × NIST 참조 에너지 50개 = **1000** 런. `energy_nist.mac`이 `nist_reference.csv`의 50 에너지와 완전히 일치하도록 구성되어 있습니다.
- 총합 약 **2550** 런의 샘플이 `transmission_summary.csv`에 누적되며, 이후 모든 플롯/오버레이는 이 CSV를 기반으로 생성됩니다.

### 출력 파일과 CSV 열 설명
`transmission_summary.csv`에는 다음과 같은 블록이 포함됩니다.
- **지오메트리/재질**: `run_id`(Geant4 run 번호), `world_half_cm`(세계 반길이), `thickness_nm`(포일 두께), `backing_thickness_um`(백킹 두께), `backing_material`(재질 이름), `density_g_cm3`(포일 밀도), `E_keV`(입사 광자 에너지)
- **전송 통계**: `N_injected`, `N_uncollided`, `N_scattered`, `N_trans_total`, `T_counts`, `T_counts_scattered`, `T_counts_clamped`, `clamp_flag`, `sigma_T_counts`
- **감쇠 계수**: `mu_counts_per_mm`, `mu_counts_cm2_g`, `mu_calc_per_mm`, `mu_calc_cm2_g`, `mu_tr_per_mm`, `mu_tr_cm2_g`, `mu_eff_per_mm`, `mu_eff_cm2_g`
- **에너지 흡수 계수**:
  - CPE 기반: `mu_en_cpe_per_mm`, `mu_en_cpe_cm2_g`, `mu_en_per_mm`, `mu_en_cm2_g`
  - 포일 기준 원시값: `mu_en_raw_per_mm`, `mu_en_raw_cm2_g`
  - 포일+백킹 원시값: `mu_en_raw_slab_per_mm`, `mu_en_raw_slab_cm2_g`
- **참조 및 잔차**: `mu_ref_cm2_g`, `mu_en_ref_cm2_g`, `delta_mu_percent`, `delta_mu_en_percent`, `delta_mu_en_cpe_percent`, `delta_mu_counts_vs_mu_calc_percent`, `delta_mu_en_cpe_vs_mu_tr_percent`, `sigma_mu_counts_cm2_g`
- **에너지 분해**: `E_trans_unc_keV`, `E_trans_tot_keV`, `E_abs_keV`(포일), `E_abs_backing_keV`, `E_abs_slab_keV`, `E_abs_other_keV`, `T_energy_unc`, `T_energy_tot`, `absorbed_fraction`, `absorbed_fraction_slab`

해당 열들은 `plot_coefficients.py`와 `generate_nist_error.py`에서 자동으로 감지됩니다. 새로운 열을 사용하지 않는 기존 스크립트도 계속 동작합니다.

### 분석 절차
- `plot_coefficients.py --csv transmission_summary.csv --reference nist_reference.csv --output-dir plots_variants --show-raw-foil --show-raw-slab`
  - 두께별 로그-로그 그래프를 생성하며, 그래프 상단에는 시뮬레이션 곡선과 NIST 곡선/포인트가 함께 표시되고 하단에는 백분율 잔차 그래프가 추가됩니다.
  - `--show-raw-foil`, `--show-raw-slab` 플래그로 포일/슬랩 원시 μ_en/ρ 곡선을 동시에 볼 수 있습니다.
  - 참조 파일을 제공하면 `plots_variants/errors/`에 두께별 오차 표를 저장합니다.
  - 그래프 읽는 법:
    - 상단 패널: 굵은 실선이 시뮬레이션 μ/ρ (counts), 점선이 μ_en/ρ(CPE)/μ_eff/ρ입니다. 검은 원/사각 마커는 시뮬레이션 포인트, 빨간 원/마름모는 NIST 데이터를 의미합니다.
    - 하단 패널: `Δμ`와 `Δμ_en`이 (시뮬레이션 − NIST)/NIST [%]로 계산되어 ±10% 범위에서 얼마나 오차가 줄어드는지를 보여 줍니다.
    - x축은 keV, y축은 cm²/g 단위입니다. Foil-only vs 백킹 플롯을 비교하면 백킹이 μ_en_raw을 얼마나 보정해주는지 직관적으로 확인할 수 있습니다.
- `generate_nist_error.py`는 최신 런과 참조 값을 결합한 표를 제공합니다.
- `overlay_best_thickness.py --csv transmission_summary.csv --reference nist_reference.csv --output overlay_best.csv --plot overlay.png`
  - 각 에너지에서 `|Δμ|`와 `|Δμ_en|`의 가중 합을 최소화하는 두께·백킹 조합을 고르고, 최적 포인트를 색상/마커로 표시한 플롯을 생성합니다.
- `optimize_thickness.py --csv ... --reference ... --output optimized.csv --macro-output mac/benchmark_auto.mac`
  - 위와 동일한 스코어링을 사용하되, 최적 조합 표와 선택적인 매크로 스텁을 생성합니다. 생성된 매크로는 `/det/setBackingMaterial`, `/det/setWThickness`, `/control/alias nPrimaries` 등을 자동으로 입력합니다.
- `plots_variants/` 폴더에는 두 종류의 산출물이 있습니다.
  - `plots_variants/mu_vs_energy_<두께>nm_backing<값>um_keV.png`: 각 두께/백킹 조합에 대해 μ/ρ(전송), μ_en/ρ(CPE), μ_en/ρ(raw foil/slab), μ_eff/ρ가 에너지에 따라 어떻게 변하는지 보여주는 로그-로그 그래프입니다. 하단 잔차 서브플롯은 NIST 대비 편차를 시각화하며, 포일이 얇을수록 포토전 영역에서 raw 곡선이 크게 떨어지는 이유를 한눈에 볼 수 있습니다.
  - `plots_variants/errors/foil_only|backed/*.csv`: 위 그림에 대응하는 정량 데이터(절대/상대 오차, μ_en/raw 등)를 포함합니다. 포일 두께를 늘릴수록 Δμ가 줄어드는 구간과, 백킹을 추가했을 때 raw slab 값이 NIST에 수렴하는 과정을 수치적으로 검증할 수 있습니다.
- 최적 두께 오버레이는 프로젝트 루트의 `overlay_best_thickness.png`에 저장됩니다. 이 그래프는 NIST μ/ρ·μ_en/ρ 곡선 위에 각 에너지에서 가장 작은 (|Δμ|+|Δμ_en|) 점을 색으로 표시하며, 색상은 포일 두께( nm ), 마커 형태는 백킹 존재 여부를 나타냅니다. 대응 CSV는 `overlay_best_thickness.csv`입니다.
  - 저에너지(포토전)에서는 얇은 포일만으로도 E_dep가 충분하므로 파란/초록색 원(백킹 없음)이 최적점으로 선택됩니다.
  - 100 keV 이상으로 올라갈수록 콤프턴·쌍생성이 지배해 2차 입자 범위가 길어지므로, 최적점이 점차 노랑/주황/빨강 색상(수백 nm~수백 µm)으로 이동하고 백킹을 포함한 사각 마커로 바뀝니다. 이것이 “고에너지일수록 두꺼운 포일 + 백킹이 필요하다”는 물리적 직관과 일치합니다.

- `validate_summary.py --csv transmission_summary.csv --reference nist_reference.csv --output coverage_report.csv`
  - (선택) 플로팅 전에 실행하면 각 두께/백킹 조합이 몇 개의 에너지 포인트를 갖고 있는지, 클램프된 런은 없는지, NIST 에너지 중 몇 개를 커버했는지 확인할 수 있습니다.
- `rank_thickness_accuracy.py --csv transmission_summary.csv --reference nist_reference.csv --output thickness_accuracy_rankings.csv --best-output best_thickness_configs.csv --copy-best-plots`
  - 전체 에너지 범위에서 RMS |Δμ|와 RMS |Δμ_en|을 동시에 최소화하는 조합을 정렬합니다. Foil-only와 백킹 조합 각각의 1위는 `best_thickness_configs.csv`에 정리되며, 대응 PNG/오차 CSV가 `plots_variants/best/`에 자동으로 복사되어 발표 슬라이드에 바로 사용할 수 있습니다.

### 권장 실행 순서
```bash
rm -f transmission_summary.csv
source /home/majo/Geant4/install/geant4-v11.3.2/bin/geant4.sh
./build/attenuation mac/thickness_scan.mac
./build/attenuation mac/benchmark_compare.mac
./build/attenuation mac/nist_scan.mac
python plot_coefficients.py --csv transmission_summary.csv --reference nist_reference.csv --output-dir plots_variants
# 옵션: 두께 최적화 및 오버레이
python overlay_best_thickness.py --csv transmission_summary.csv --reference nist_reference.csv --output overlay_best.csv --plot overlay_best.png
python optimize_thickness.py --csv transmission_summary.csv --reference nist_reference.csv --macro-output mac/benchmark_auto.mac
python generate_nist_error.py transmission_summary.csv nist_reference.csv plots_variants/nist_error_summary.csv
# 선택: 품질 보증과 평균 성능 요약
python validate_summary.py --csv transmission_summary.csv --reference nist_reference.csv --output coverage_report.csv
python rank_thickness_accuracy.py --csv transmission_summary.csv --reference nist_reference.csv --output thickness_accuracy_rankings.csv --best-output best_thickness_configs.csv --copy-best-plots --plots-dir plots_variants --best-plots-dir plots_variants/best
```
이 과정을 통해 백킹을 포함한 구성이든, 백킹을 제거한 구성이든 동일한 NIST 에너지 집합에서 시뮬레이션 값을 추출하고 오차 테이블을 생성할 수 있습니다. `mac/thickness_scan.mac`이 모든 세계/두께 조합을 채우고, `mac/benchmark_compare.mac`과 `mac/nist_scan.mac`이 비교·참조용 런을 추가합니다. 플롯은 `plots_variants/foil_only|backed/` 아래에 정리되며, `plots_variants/best/`에는 RMS 기준으로 가장 NIST에 근접한 Foil-only/백킹 조합의 PNG·오차 CSV가 자동으로 복사됩니다. 최적 두께 곡선은 `overlay_best_thickness.png`/`overlay_best_thickness.csv`로 확인하고, `optimized_thicknesses.csv`·`mac/benchmark_auto.mac`을 이용해 동일 조건을 재실행할 수 있습니다. `plots_variants/nist_error_summary.csv`는 에너지별 최신 결과를 NIST 표와 1:1로 비교한 요약본입니다.

### basic 빌드와의 차이
- variants는 `/det/setBackingThickness` 및 `/det/setBackingMaterial` 명령을 통해 Foil 뒤에 원하는 재질/두께의 백킹을 둘 수 있으며, `SteppingAction`이 Foil→Backing→World 경계 각각에서 한 번만 투과를 기록합니다.
- `TrackInfo`에 `TransmissionLogged` 플래그를 추가해 다중 영역을 통과하는 포톤을 중복 집계하지 않습니다.
- `RunAction`은 포일/백킹/기타 영역의 에너지 침적을 분리해 μ_en/ρ(raw)와 μ_en/ρ(raw slab)를 모두 제공합니다.
- 나머지 물리 리스트, 난수 시드, CSV 스키마, 플로팅 파이프라인은 기본 라인과 동일합니다.

---

## English Version

### Folder Highlights
```
variants/
├── mac/
│   ├── thickness_scan.mac        <-- 2×9×(46/18) energy sweeps
│   ├── benchmark_compare.mac     <-- Foil-only vs backed (4 energies × 2 backings)
│   └── benchmark_auto.mac        <-- Auto-generated replay macro from optimizer
├── transmission_summary.csv
├── plots_variants/
│   ├── foil_only/                <-- PNGs with backing disabled
│   ├── backed/                   <-- PNGs with the 50 µm backing
│   ├── best/                     <-- Top performers copied by rank_thickness_accuracy.py
│   └── errors/
│       ├── foil_only/
│       └── backed/
├── overlay_best_thickness.{png,csv}
├── optimized_thicknesses.csv
├── best_thickness_configs.csv
└── analysis scripts + QA outputs (coverage_report.csv, thickness_accuracy_rankings.csv, …)
```

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

#### CPE vs. raw μ_en/ρ
- **`μ_en/ρ (raw, foil)`** – derived strictly from the energy deposited inside the foil. Thin foils underestimate the NIST value because electrons flee the slab before depositing their energy.
- **`μ_en/ρ (raw, slab)`** – uses the combined foil + backing energy deposition and mass path. As the backing becomes thicker than the electron ranges, it approaches the NIST limit.
- **`μ_en/ρ (CPE)`** – scales `μ_counts` by `(μ_en/μ)_NIST` (or `μ_tr` if no reference is available) to emulate the infinite-medium definition used by NIST.

Pass `--show-raw-foil` or `--show-raw-slab` to `plot_coefficients.py` to visualize all three curves together and see how quickly a given backing configuration restores charged-particle equilibrium.

#### New diagnostics and controls
- `/det/setBackingMaterial <name>` lets you swap the backing slab to any material available in Geant4 (default `G4_W`).
- `W_USE_LOG_INTERP=1` switches the NIST interpolation routine to log–log space instead of linear.
- `W_USE_MU_TR_CALC=0` disables the fallback that replaces `μ_en/ρ` with `μ_tr/ρ` when no NIST reference row is available.
- `W_COMPTON_STEPS=<even>` refines or loosens the Simpson integration used for the Compton retention term (default 512 steps).
- New CSV columns `T_counts_clamped` and `clamp_flag` show when the transmission fraction had to be clamped away from 0 or 1 before logarithms were taken.
- `N_scattered`, `T_counts_scattered` keep track of downstream transmissions that underwent at least one interaction, complementing the legacy `T_counts` (uncollided only).
- Energy deposition is split into foil/backing/other contributions, exposing `E_abs_backing_keV`, `E_abs_slab_keV`, `E_abs_other_keV`, `absorbed_fraction_slab`, and the corresponding raw μ_en/ρ columns.
- Additional diagnostics `delta_mu_counts_vs_mu_calc_percent` and `delta_mu_en_cpe_vs_mu_tr_percent` compare the transmission-derived coefficients against `G4EmCalculator`.

#### What does the backing represent?
- The “backing” is a downstream slab (50 µm tungsten by default) that prevents secondary electrons from fleeing into vacuum, restoring charged-particle equilibrium so `(μ_en/ρ)_raw (slab)` approaches the NIST definition.
- `/det/setBackingMaterial` lets you swap this slab to any Geant4 material to study how different stopping powers affect μ_en trends.
- In `overlay_best_thickness.png` the marker shape shows whether a data point used a backing (squares) or a bare foil (circles), so you can immediately tell which regime is closer to NIST.

#### Geometry and beam configuration
- `mac/init.mac` always seeds `/random/setSeeds 123456 789012` so foil-only vs backed runs share identical RNG streams. That makes percentage deltas meaningful without chasing statistical fluctuations.
- The GPS plane source is a 0.5 mm radius disk 25 cm upstream of the foil, firing exactly along +z. This recreates the narrow, almost parallel beam assumed by NIST XCOM tables.
- World half-lengths of 50 cm and 100 cm were chosen so scattered photons/electrons are absorbed before they can re-enter the foil. Smaller worlds would artificially boost backscatter and spoil the narrow-beam comparison.
- `SteppingAction` records a transmission the instant a photon crosses Foil→Backing or (Foil|Backing)→World while traveling downstream, then kills the track. No photon can re-enter the foil, ensuring each primary contributes at most one row in the transmission tally.

#### Thickness/backing heuristics
- The canonical thicknesses hard-coded in `mac/benchmark_core.mac` (200 nm at 60 keV, 300 nm at 80 keV, 250 µm at 500 keV, 750 µm at 1 MeV) keep the optical depth τ = μρt between roughly 0.3 and 3. That keeps `T_counts` away from 0 or 1, where the logarithm would amplify counting noise.
- The default 50 µm tungsten backing is thick enough to stop most secondary electrons up to ≈1 MeV, restoring charged-particle equilibrium without inflating the world volume. Use `/det/setBackingThickness`, `/det/setBackingMaterial`, or the optimization scripts whenever a different slab better matches your energy of interest.
- `mac/thickness_scan.mac` and `mac/nist_scan.mac` cover a grid of world sizes (50/100 cm) and foil thicknesses (100 nm–10 µm) so you can empirically see how τ and backing choices affect Δμ and Δμ_en before plotting or generating overlays.
- **Physical intuition**:
  - At low energies (≲100 keV) the photoelectric effect dominates; a single absorption deposits almost all energy locally, so thin foils (100–300 nm) capture enough interactions without driving `T_counts` to zero.
  - In the mid-energy regime (100 keV–1 MeV) Compton scattering governs attenuation. Thicker foils (hundreds of nm to hundreds of µm) keep `T_counts` in a numerically stable range while the 50 µm backing catches secondary electrons whose ranges grow with energy.
  - Above ≈1 MeV, pair production and long-range electrons appear, μ/ρ shrinks, and optical depth stays reasonable only when the foil is at least a few hundred µm thick. Swapping the backing material (e.g., Al vs Cu vs W) directly shows how stopping power affects μ_en trends.
  - `overlay_best_thickness.py` distills these heuristics into a composite “best achievable” curve (see `overlay_best_thickness.{png,csv}`), and `optimize_thickness.py` emits `mac/benchmark_auto.mac` so you can replay the winning combinations without combing through the full scan.

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
- `mac/thickness_scan_foil.mac` – same grid as above but keeps `/det/setBackingThickness 0 um` throughout, so foil-only runs cover the same energy span for direct comparison.
- `mac/nist_scan.mac` – same geometry grid but restricted to the NIST energy list for one-to-one comparisons.

#### Run statistics (Geant4 11.3.2, HybridEmPhysics, 200 k primaries unless noted)
- `thickness_scan.mac`: 2 world sizes × [(8 thicknesses × 46 energies from `energy_full.mac`) + (10 µm × 18 energies from `energy_high.mac`)] = **772** runs, all with the 50 µm backing.
- `benchmark_compare.mac`: 4 sentinel energies (60, 80, 500, 1000 keV) × 2 backings = **8** paired foil/backing runs that share identical RNG streams.
- `nist_scan.mac`: 2 world sizes × 10 thicknesses × 50 NIST energies = **1000** runs, guaranteeing one-to-one coverage of the `nist_reference.csv` table.
- Combined, the master CSV aggregates roughly **1780** runs before any post-processing.

### Output files and column glossary
`transmission_summary.csv` now exposes:
- **Geometry & material**: `run_id`, `world_half_cm`, `thickness_nm`, `backing_thickness_um`, `backing_material`, `density_g_cm3`, `E_keV`
- **Transmission stats**: `N_injected`, `N_uncollided`, `N_scattered`, `N_trans_total`, `T_counts`, `T_counts_scattered`, `T_counts_clamped`, `clamp_flag`, `sigma_T_counts`
- **Attenuation coefficients**: `mu_counts_per_mm`, `mu_counts_cm2_g`, `mu_calc_per_mm`, `mu_calc_cm2_g`, `mu_tr_per_mm`, `mu_tr_cm2_g`, `mu_eff_per_mm`, `mu_eff_cm2_g`
- **Energy-absorption coefficients**:
  - CPE-adjusted: `mu_en_cpe_per_mm`, `mu_en_cpe_cm2_g`, `mu_en_per_mm`, `mu_en_cm2_g`
  - Raw foil: `mu_en_raw_per_mm`, `mu_en_raw_cm2_g`
  - Raw slab: `mu_en_raw_slab_per_mm`, `mu_en_raw_slab_cm2_g`
- **Reference & residuals**: `mu_ref_cm2_g`, `mu_en_ref_cm2_g`, `delta_mu_percent`, `delta_mu_en_percent`, `delta_mu_en_cpe_percent`, `delta_mu_counts_vs_mu_calc_percent`, `delta_mu_en_cpe_vs_mu_tr_percent`, `sigma_mu_counts_cm2_g`
- **Energy balance**: `E_trans_unc_keV`, `E_trans_tot_keV`, `E_abs_keV` (foil), `E_abs_backing_keV`, `E_abs_slab_keV`, `E_abs_other_keV`, `T_energy_unc`, `T_energy_tot`, `absorbed_fraction`, `absorbed_fraction_slab`

### Analysis tools
- `plot_coefficients.py --csv transmission_summary.csv --reference nist_reference.csv --output-dir plots_variants`
  - Produces per-thickness log–log plots with both the simulation curves and NIST curves/points, plus a residual subplot that charts the percentage differences.
  - Pass `--show-raw-foil` or `--show-raw-slab` to overlay the raw μ_en/ρ curves alongside the CPE-adjusted one.
  - Writes detailed error tables to `plots_variants/errors/` when a reference CSV is provided, reporting absolute and percentage deviations for μ/ρ and μ_en/ρ (CPE + raw foil/slab).
- `generate_nist_error.py` – compresses the latest simulation row per energy into a comparison table.
- `overlay_best_thickness.py` – for each energy, pick the row that minimizes a weighted sum of |Δμ| and |Δμ_en|, then emit both a CSV and a plot that overlays those “best achievable” points on the NIST curves.
- `optimize_thickness.py` – same scoring as above, but also emits a macro stub (via `--macro-output mac/benchmark_auto.mac`) so you can replay only the winning combinations.
- `plots_variants/mu_vs_energy_<thickness>nm_backing<value>um_keV.png` visualize μ/ρ (counts), μ_en/ρ (CPE), μ_en/ρ (raw foil/slab), and μ_eff/ρ versus energy for each foil/backing pair, with a residual subplot that highlights Δμ and Δμ_en trends. Companion CSVs under `plots_variants/errors/foil_only|backed/` list the exact deviations so you can quantify how extra backing or thicker foils affect agreement in photoelectric-, Compton-, or pair-production–dominated regimes.
- The best-thickness overlay lives at `overlay_best_thickness.png`: colored markers encode the foil thickness (log scale) while marker shape distinguishes foil-only vs backed configurations. The underlying data table is `overlay_best_thickness.csv`, and `optimized_thicknesses.csv` plus `mac/benchmark_auto.mac` give you a replayable list of those optimal settings.
  - At low energies the optimal points remain cool-colored circles because photoelectric absorption already enforces CPE without a backing. As energy rises, the markers migrate toward warmer colors (thicker foils) and switches to squares, signalling that Compton/pair-production regimes require both thicker foils and backing slabs to keep μ_en/ρ close to NIST.
- `validate_summary.py` – sanity-checks coverage before plotting; run it to confirm every (world, thickness, backing) combination has enough energy points and no NaN μ/ρ values.
- `rank_thickness_accuracy.py --best-output best_thickness_configs.csv --copy-best-plots --plots-dir plots_variants --best-plots-dir plots_variants/best` – ranks each foil/backing combination by RMS |Δμ| and |Δμ_en| so you can see which configurations agree with NIST over the broadest energy range. The top foil-only and backed entries are written to `best_thickness_configs.csv`, and their PNG/error CSVs are copied into `plots_variants/best/` for presentations.

Recommended workflow:
```bash
rm -f transmission_summary.csv
source /home/majo/Geant4/install/geant4-v11.3.2/bin/geant4.sh
./build/attenuation mac/thickness_scan.mac
./build/attenuation mac/benchmark_compare.mac
./build/attenuation mac/nist_scan.mac
python plot_coefficients.py --csv transmission_summary.csv --reference nist_reference.csv --output-dir plots_variants
python overlay_best_thickness.py --csv transmission_summary.csv --reference nist_reference.csv --output overlay_best.csv --plot overlay_best.png
python optimize_thickness.py --csv transmission_summary.csv --reference nist_reference.csv --macro-output mac/benchmark_auto.mac
python generate_nist_error.py transmission_summary.csv nist_reference.csv plots_variants/nist_error_summary.csv
python validate_summary.py --csv transmission_summary.csv --reference nist_reference.csv --output coverage_report.csv
python rank_thickness_accuracy.py --csv transmission_summary.csv --reference nist_reference.csv --output thickness_accuracy_rankings.csv --best-output best_thickness_configs.csv --copy-best-plots --plots-dir plots_variants --best-plots-dir plots_variants/best
```
This produces a reproducible data set aligned with the NIST energy list plus error tables that are ready for slide decks or reports. `mac/thickness_scan.mac` fills the world/thickness grid, the benchmark & NIST macros add foil-only/backed reference runs, `plots_variants/foil_only|backed/` collects per-thickness PNG/CSV files, and the top-performing configurations are mirrored to `plots_variants/best/`. Overlay artifacts live in `overlay_best_thickness.{png,csv}`, while `optimized_thicknesses.csv` and `mac/benchmark_auto.mac` let you replay those settings. The compressed NIST comparison table is written to `plots_variants/nist_error_summary.csv`.

### Relationship to the basic line
- Variants exposes `/det/setBackingThickness` and `/det/setBackingMaterial`, and records foil/backing transmission separately so each photon is logged exactly once as it exits Foil→Backing→World.
- Track-level bookkeeping prevents double counting, and `RunAction` splits energy deposition into foil/backing components so both raw and slab μ_en/ρ values are available.
- The HybridEmPhysics list, deterministic seeding, CSV schema, and plotting pipeline remain shared with the basic build so any differences stem solely from the intentional geometry/stepping changes.
