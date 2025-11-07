#!/usr/bin/env python3
"""Generate a comparison table between simulation outputs and NIST reference data.

The script interpolates the NIST coefficients to the simulation energies so that
errors can be computed even when the sampled energies do not exactly match the
tabulated ones.
"""

import argparse
from pathlib import Path
from typing import Iterable, Optional

import numpy as np
import pandas as pd

def main() -> None:
    parser = argparse.ArgumentParser(
        description=(
            "Compare simulation attenuation coefficients with NIST data. "
            "The NIST table is used as the reference and is interpolated onto "
            "the simulation energies before computing relative errors."
        )
    )
    parser.add_argument("summary", type=Path, help="Path to transmission_summary.csv")
    parser.add_argument("reference", type=Path, help="Path to nist_reference.csv")
    parser.add_argument("output", type=Path, help="Output CSV path")
    args = parser.parse_args()

    if not args.reference.exists():
        raise FileNotFoundError(args.reference)
    if not args.summary.exists():
        raise FileNotFoundError(args.summary)

    reference = (
        pd.read_csv(args.reference)
        .rename(
            columns={
                "E_keV": "E_keV",
                "mu_over_rho_cm2_g": "mu_ref_cm2_g",
                "mu_en_over_rho_cm2_g": "mu_en_ref_cm2_g",
            }
        )
        .dropna(subset=["E_keV", "mu_ref_cm2_g", "mu_en_ref_cm2_g"])
        .sort_values("E_keV")
        .reset_index(drop=True)
    )
    reference["E_keV"] = reference["E_keV"].astype(float)
    reference["mu_ref_cm2_g"] = reference["mu_ref_cm2_g"].astype(float)
    reference["mu_en_ref_cm2_g"] = reference["mu_en_ref_cm2_g"].astype(float)

    sim = pd.read_csv(args.summary)
    if "E_keV" not in sim.columns:
        raise ValueError("summary file must contain an E_keV column")

    sim = sim.copy()
    sim["E_keV"] = sim["E_keV"].astype(float)

    attenuation_col = _first_present(sim.columns, ["mu_counts_cm2_g", "mu_calc_cm2_g", "mu_tr_cm2_g"])
    energy_abs_col = _first_present(
        sim.columns,
        [
            "mu_en_cpe_cm2_g",
            "mu_en_cm2_g",
            "mu_en_raw_cm2_g",
        ],
    )

    if attenuation_col is None:
        raise ValueError(
            "summary file must contain one of the attenuation columns: "
            "mu_counts_cm2_g, mu_calc_cm2_g, or mu_tr_cm2_g"
        )

    ref_energies = reference["E_keV"].to_numpy()
    ref_mu = reference["mu_ref_cm2_g"].to_numpy()
    ref_mu_en = reference["mu_en_ref_cm2_g"].to_numpy()

    sim["mu_ref_cm2_g"] = _interpolate(ref_energies, ref_mu, sim["E_keV"])
    if energy_abs_col is not None:
        sim["mu_en_ref_cm2_g"] = _interpolate(ref_energies, ref_mu_en, sim["E_keV"])

    sim["delta_mu_percent"] = _percent_delta(sim[attenuation_col], sim["mu_ref_cm2_g"])
    if energy_abs_col is not None:
        sim["delta_mu_en_percent"] = _percent_delta(sim[energy_abs_col], sim["mu_en_ref_cm2_g"])

    exact_match = np.isclose(sim["E_keV"].to_numpy()[:, None], ref_energies, rtol=0.0, atol=1e-6).any(axis=1)
    sim["nist_exact_energy_match"] = exact_match

    base_cols = [col for col in ["run_id", "world_half_cm", "thickness_nm", "density_g_cm3", "E_keV"] if col in sim.columns]

    output_cols = base_cols + [
        attenuation_col,
        "mu_ref_cm2_g",
        "delta_mu_percent",
        "nist_exact_energy_match",
    ]
    if energy_abs_col is not None:
        output_cols.extend(
            [
                energy_abs_col,
                "mu_en_ref_cm2_g",
                "delta_mu_en_percent",
            ]
        )

    out_df = sim[output_cols].copy()
    rename_map = {attenuation_col: "mu_sim_cm2_g"}
    if energy_abs_col is not None:
        rename_map[energy_abs_col] = "mu_en_sim_cm2_g"
    out_df = out_df.rename(columns=rename_map)
    numeric_cols = out_df.select_dtypes(include=["number"]).columns
    out_df[numeric_cols] = out_df[numeric_cols].round(6)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    out_df.to_csv(args.output, index=False)


def _first_present(columns: Iterable[str], candidates: Iterable[str]) -> Optional[str]:
    for candidate in candidates:
        if candidate in columns:
            return candidate
    return None


def _interpolate(ref_energy: np.ndarray, ref_values: np.ndarray, query_energy: Iterable[float]) -> np.ndarray:
    ref_energy = np.asarray(ref_energy, dtype=float)
    ref_values = np.asarray(ref_values, dtype=float)
    query_energy = np.asarray(query_energy, dtype=float)

    use_log = (
        np.all(ref_energy > 0)
        and np.all(ref_values > 0)
        and np.all(query_energy > 0)
    )
    if use_log:
        log_ref_energy = np.log(ref_energy)
        log_ref_values = np.log(ref_values)
        log_query = np.log(query_energy)
        left = log_ref_values[0]
        right = log_ref_values[-1]
        interpolated = np.exp(np.interp(log_query, log_ref_energy, log_ref_values, left=left, right=right))
    else:
        interpolated = np.interp(query_energy, ref_energy, ref_values, left=ref_values[0], right=ref_values[-1])
    return interpolated


def _percent_delta(sim_values: pd.Series, ref_values: pd.Series) -> pd.Series:
    sim_arr = sim_values.astype(float)
    ref_arr = ref_values.astype(float)
    with np.errstate(divide="ignore", invalid="ignore"):
        delta = (sim_arr - ref_arr) / ref_arr * 100.0
    delta[~np.isfinite(delta)] = np.nan
    return delta


if __name__ == "__main__":
    main()
