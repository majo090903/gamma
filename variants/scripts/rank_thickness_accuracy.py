#!/usr/bin/env python3
"""Rank foil/backing configurations by their agreement with NIST reference data."""

import argparse
import shutil
from pathlib import Path
from typing import Optional

import numpy as np
import pandas as pd


def log_interp(query: np.ndarray, ref_energy: np.ndarray, ref_values: np.ndarray) -> np.ndarray:
    ref_energy = np.asarray(ref_energy, dtype=float)
    ref_values = np.asarray(ref_values, dtype=float)
    query = np.asarray(query, dtype=float)
    if np.any(ref_energy <= 0.0) or np.any(ref_values <= 0.0):
        return np.interp(query, ref_energy, ref_values, left=ref_values[0], right=ref_values[-1])
    log_ref_energy = np.log(ref_energy)
    log_ref_values = np.log(ref_values)
    log_query = np.log(query)
    return np.exp(np.interp(log_query, log_ref_energy, log_ref_values, left=log_ref_values[0], right=log_ref_values[-1]))


def percent_delta(sim: pd.Series, ref: pd.Series) -> pd.Series:
    sim = sim.astype(float)
    ref = ref.astype(float)
    with np.errstate(divide="ignore", invalid="ignore"):
        delta = (sim - ref) / ref * 100.0
    delta[~np.isfinite(delta)] = np.nan
    return delta


def main() -> None:
    parser = argparse.ArgumentParser(description="Score each thickness/backing combo against NIST.")
    parser.add_argument("--csv", default="transmission_summary.csv", help="Path to transmission_summary.csv.")
    parser.add_argument("--reference", default="nist_reference.csv", help="Path to nist_reference.csv.")
    parser.add_argument("--output", default="thickness_accuracy_rankings.csv", help="Output CSV path.")
    parser.add_argument(
        "--best-output",
        default="best_thickness_configs.csv",
        help="CSV containing the top foil-only and backed configurations.",
    )
    parser.add_argument("--plots-dir", default="plots_variants", help="Directory where plot PNG/CSV files live.")
    parser.add_argument(
        "--best-plots-dir",
        default="plots_variants/best",
        help="Destination folder where the best PNG/CSV files will be copied.",
    )
    parser.add_argument(
        "--copy-best-plots",
        action="store_true",
        help="Copy the PNG/error CSV for the best foil-only and backed configs into --best-plots-dir.",
    )
    parser.add_argument("--world", type=float, default=None, help="Optional world_half_cm filter.")
    parser.add_argument("--min-energies", type=int, default=1, help="Minimum distinct energies per group.")
    parser.add_argument("--allow-clamped", action="store_true", help="Include rows where clamp_flag != 0.")
    parser.add_argument("--weight-mu", type=float, default=0.5, help="Weight for |Δμ/ρ| RMS in the score.")
    parser.add_argument("--weight-muen", type=float, default=0.5, help="Weight for |Δμ_en/ρ| RMS in the score.")
    args = parser.parse_args()

    df = pd.read_csv(args.csv)
    required = {"world_half_cm", "thickness_nm", "backing_thickness_um", "E_keV", "mu_counts_cm2_g"}
    missing = required - set(df.columns)
    if missing:
        raise ValueError(f"{args.csv} is missing columns: {missing}")

    if args.world is not None:
        df = df[df["world_half_cm"] == args.world]
        if df.empty():
            raise ValueError(f"No rows for world_half_cm == {args.world}")

    if not args.allow_clamped and "clamp_flag" in df.columns:
        df = df[df["clamp_flag"] == 0]

    ref_df = pd.read_csv(args.reference)
    if not {"E_keV", "mu_over_rho_cm2_g", "mu_en_over_rho_cm2_g"}.issubset(ref_df.columns):
        raise ValueError(f"{args.reference} must contain E_keV, mu_over_rho_cm2_g, mu_en_over_rho_cm2_g")
    ref_df = ref_df.sort_values("E_keV").reset_index(drop=True)
    mu_ref = log_interp(df["E_keV"], ref_df["E_keV"], ref_df["mu_over_rho_cm2_g"])
    mu_en_ref = log_interp(df["E_keV"], ref_df["E_keV"], ref_df["mu_en_over_rho_cm2_g"])

    df = df.copy()
    df["mu_ref_cm2_g"] = mu_ref
    df["mu_en_ref_cm2_g"] = mu_en_ref
    if "delta_mu_percent" not in df.columns:
        df["delta_mu_percent"] = percent_delta(df["mu_counts_cm2_g"], df["mu_ref_cm2_g"])
    if "delta_mu_en_cpe_percent" not in df.columns and "mu_en_cpe_cm2_g" in df.columns:
        df["delta_mu_en_cpe_percent"] = percent_delta(df["mu_en_cpe_cm2_g"], df["mu_en_ref_cm2_g"])

    groups = []
    for (world, thickness, backing), sub in df.groupby(["world_half_cm", "thickness_nm", "backing_thickness_um"]):
        energies = len(pd.unique(sub["E_keV"].round(6)))
        if energies < args.min_energies:
            continue
        rms_mu = sub["delta_mu_percent"].dropna().pow(2).mean() ** 0.5
        rms_muen = sub["delta_mu_en_cpe_percent"].dropna().pow(2).mean() ** 0.5
        score = args.weight_mu * rms_mu + args.weight_muen * rms_muen
        clamp_series = sub["clamp_flag"] if "clamp_flag" in sub.columns else pd.Series(0, index=sub.index)
        groups.append(
            {
                "world_half_cm": world,
                "thickness_nm": thickness,
                "backing_thickness_um": backing,
                "energies": energies,
                "rms_delta_mu_percent": rms_mu,
                "rms_delta_mu_en_percent": rms_muen,
                "score": score,
                "E_min": sub["E_keV"].min(),
                "E_max": sub["E_keV"].max(),
                "rows": len(sub),
                "rows_clamped": int((clamp_series != 0).sum()),
            }
        )

    if not groups:
        raise ValueError("No groups met the minimum energy requirement; rerun the simulation or lower --min-energies.")

    ranking = pd.DataFrame(groups).sort_values("score").reset_index(drop=True)
    ranking.to_csv(args.output, index=False)
    print(f"[info] Saved ranking to {args.output}")
    print(ranking.head(10).to_string(index=False))

    best_rows = []
    if not ranking.empty:
        best_rows.append(ranking.iloc[[0]])
    best_no_backing = ranking[np.isclose(ranking["backing_thickness_um"], 0.0)].head(1)
    if not best_no_backing.empty:
        best_rows.append(best_no_backing)
    best_backed = ranking[ranking["backing_thickness_um"] > 0].head(1)
    if not best_backed.empty:
        best_rows.append(best_backed)
    if best_rows:
        best_summary = pd.concat(best_rows).drop_duplicates().reset_index(drop=True)
        Path(args.best_output).parent.mkdir(parents=True, exist_ok=True)
        best_summary.to_csv(args.best_output, index=False)
        print(f"[info] Saved best configs to {args.best_output}")

        if args.copy_best_plots:
            copy_best_plots(best_summary, Path(args.plots_dir), Path(args.best_plots_dir))


def copy_best_plots(best_summary: pd.DataFrame, plots_dir: Path, best_dir: Path) -> None:
    best_dir.mkdir(parents=True, exist_ok=True)

    def format_label(value: float) -> str:
        return f"{value:g}".replace(".", "_")

    for row in best_summary.itertuples(index=False):
        thickness_label = format_label(row.thickness_nm)
        backing_label = format_label(row.backing_thickness_um)
        subdir = "foil_only" if np.isclose(row.backing_thickness_um, 0.0) else "backed"
        png_found = False
        for suffix in ("keV", "MeV"):
            candidate = plots_dir / subdir / f"mu_vs_energy_{thickness_label}nm_backing{backing_label}um_{suffix}.png"
            if candidate.exists():
                shutil.copy2(candidate, best_dir / candidate.name)
                print(f"[info] Copied {candidate} -> {best_dir / candidate.name}")
                png_found = True
                break
        if not png_found:
            print(f"[warn] Missing plot for thickness {row.thickness_nm} nm, backing {row.backing_thickness_um} µm.")

        source_csv = plots_dir / "errors" / subdir / f"errors_{thickness_label}nm_backing{backing_label}um.csv"

        if source_csv.exists():
            shutil.copy2(source_csv, best_dir / source_csv.name)
            print(f"[info] Copied {source_csv} -> {best_dir / source_csv.name}")
        else:
            print(f"[warn] Missing {source_csv}; skipping copy.")


if __name__ == "__main__":
    main()
