#!/usr/bin/env python3
"""Build a composite curve that picks the best-performing thickness/backing per energy."""

import argparse
from pathlib import Path
from typing import Optional

import matplotlib.pyplot as plt
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


def load_reference(path: Path) -> pd.DataFrame:
    if not path.exists():
        raise FileNotFoundError(f"Reference CSV {path} not found.")
    ref = pd.read_csv(path)
    required = {"E_keV", "mu_over_rho_cm2_g", "mu_en_over_rho_cm2_g"}
    if not required.issubset(ref.columns):
        missing = required - set(ref.columns)
        raise ValueError(f"Reference CSV missing columns: {missing}")
    return ref.rename(
        columns={
            "mu_over_rho_cm2_g": "mu_ref_cm2_g",
            "mu_en_over_rho_cm2_g": "mu_en_ref_cm2_g",
        }
    ).sort_values("E_keV")


def attach_reference_columns(df: pd.DataFrame, ref: pd.DataFrame) -> pd.DataFrame:
    result = df.copy()
    if "mu_ref_cm2_g" not in result.columns:
        result["mu_ref_cm2_g"] = log_interp(result["E_keV"], ref["E_keV"], ref["mu_ref_cm2_g"])
    if "mu_en_ref_cm2_g" not in result.columns:
        result["mu_en_ref_cm2_g"] = log_interp(result["E_keV"], ref["E_keV"], ref["mu_en_ref_cm2_g"])
    if "delta_mu_percent" not in result.columns:
        result["delta_mu_percent"] = percent_delta(result["mu_counts_cm2_g"], result["mu_ref_cm2_g"])
    if "delta_mu_en_cpe_percent" not in result.columns and "mu_en_cpe_cm2_g" in result.columns:
        result["delta_mu_en_cpe_percent"] = percent_delta(result["mu_en_cpe_cm2_g"], result["mu_en_ref_cm2_g"])
    return result


def filter_rows(
    df: pd.DataFrame,
    min_transmission: float,
    max_transmission: float,
    min_primaries: int,
    allow_clamped: bool,
    world: Optional[float] = None,
) -> pd.DataFrame:
    if "T_counts" not in df.columns:
        raise ValueError("T_counts column missing from summary CSV.")
    filtered = df.copy()
    if world is not None:
        filtered = filtered[filtered["world_half_cm"] == world]
    primaries = filtered["N_injected"] if "N_injected" in filtered.columns else pd.Series(
        np.inf, index=filtered.index
    )
    mask = (
        (filtered["T_counts"] >= min_transmission)
        & (filtered["T_counts"] <= max_transmission)
        & (primaries >= min_primaries)
    )
    filtered = filtered[mask]
    if not allow_clamped and "clamp_flag" in filtered.columns:
        filtered = filtered[filtered["clamp_flag"] == 0]
    return filtered


def select_best_rows(df: pd.DataFrame, weight_mu: float, weight_muen: float) -> pd.DataFrame:
    working = df.dropna(subset=["delta_mu_percent", "delta_mu_en_cpe_percent"]).copy()
    working["score_mu"] = working["delta_mu_percent"].abs()
    working["score_muen"] = working["delta_mu_en_cpe_percent"].abs()
    working["score"] = weight_mu * working["score_mu"] + weight_muen * working["score_muen"]
    best_indices = working.groupby("E_keV")["score"].idxmin()
    best = working.loc[best_indices].sort_values("E_keV").reset_index(drop=True)
    return best


def plot_overlay(best: pd.DataFrame, ref: pd.DataFrame, output: Path, args: argparse.Namespace) -> None:
    if best.empty:
        raise ValueError("No best rows available to plot.")
    fig, (ax_mu, ax_muen) = plt.subplots(
        nrows=2,
        ncols=1,
        figsize=(8.5, 8),
        sharex=True,
        gridspec_kw={"height_ratios": [3, 2]},
    )

    ref_energy = ref["E_keV"].to_numpy()
    ax_mu.loglog(ref_energy, ref["mu_ref_cm2_g"], color="tab:gray", linestyle="--", label="NIST μ/ρ")
    ax_muen.loglog(ref_energy, ref["mu_en_ref_cm2_g"], color="tab:gray", linestyle="--", label="NIST μ_en/ρ")

    thickness = best["thickness_nm"].to_numpy()
    thickness = np.where(thickness <= 0, np.min(thickness[np.nonzero(thickness)]) if np.any(thickness > 0) else 1.0, thickness)
    log_thickness = np.log10(thickness)
    norm = plt.Normalize(log_thickness.min(), log_thickness.max())
    cmap = plt.cm.viridis
    colors = cmap(norm(log_thickness))
    for idx, row in best.iterrows():
        marker = "o" if row.get("backing_thickness_um", 0.0) <= 0.0 else "s"
        edge = "black"
        ax_mu.scatter(
            row["E_keV"],
            row["mu_counts_cm2_g"],
            color=colors[idx],
            edgecolors=edge,
            marker=marker,
            s=55,
            label="_nolegend_",
        )
        ax_muen.scatter(
            row["E_keV"],
            row["mu_en_cpe_cm2_g"],
            color=colors[idx],
            edgecolors=edge,
            marker=marker,
            s=55,
            label="_nolegend_",
        )

    sm = plt.cm.ScalarMappable(norm=norm, cmap=cmap)
    sm.set_array([])
    cbar = fig.colorbar(sm, ax=[ax_mu, ax_muen], pad=0.02)
    cbar.set_label("Foil thickness (nm)")

    legend_markers = [
        plt.Line2D([], [], marker="o", linestyle="None", markerfacecolor="none", markeredgecolor="black", label="Foil only"),
        plt.Line2D([], [], marker="s", linestyle="None", markerfacecolor="none", markeredgecolor="black", label="Foil + backing"),
    ]
    ax_mu.legend(handles=legend_markers + [ax_mu.lines[0]], loc="lower left")
    ax_muen.legend(loc="lower left")

    ax_mu.set_ylabel(r"$\mu/\rho$ (cm$^2$/g)")
    ax_mu.grid(True, which="both", linestyle=":", alpha=0.4)
    ax_mu.set_title("Composite best-thickness overlay")

    ax_muen.set_xlabel("Photon energy (keV)")
    ax_muen.set_ylabel(r"$\mu_{en}/\rho$ (cm$^2$/g)")
    ax_muen.grid(True, which="both", linestyle=":", alpha=0.4)

    subtitle = f"T window [{args.min_transmission}, {args.max_transmission}], min primaries {args.min_primaries}"
    if args.world is not None:
        subtitle += f", world {args.world} cm"
    fig.suptitle(f"Best-score overlay (w_mu={args.weight_mu}, w_mu_en={args.weight_muen})\n{subtitle}", fontsize=11)

    fig.tight_layout()
    fig.savefig(output, dpi=220)
    plt.close(fig)


def main() -> None:
    parser = argparse.ArgumentParser(description="Overlay best-performing rows per energy onto the NIST curves.")
    parser.add_argument("--csv", default="transmission_summary.csv", help="Path to transmission_summary.csv")
    parser.add_argument("--reference", default="nist_reference.csv", help="Path to nist_reference.csv")
    parser.add_argument("--output", default="overlay_best_thickness.csv", help="Output CSV with best rows.")
    parser.add_argument("--plot", default="overlay_best_thickness.png", help="Output PNG for the overlay plot.")
    parser.add_argument("--world", type=float, default=None, help="Optional world_half_cm filter.")
    parser.add_argument("--min-transmission", type=float, default=0.01, dest="min_transmission", help="Minimum T_counts.")
    parser.add_argument("--max-transmission", type=float, default=0.99, dest="max_transmission", help="Maximum T_counts.")
    parser.add_argument("--min-primaries", type=int, default=50000, dest="min_primaries", help="Minimum primaries per row.")
    parser.add_argument("--allow-clamped", action="store_true", help="Include rows where T_counts had to be clamped.")
    parser.add_argument("--weight-mu", type=float, default=0.5, help="Weight for |Δμ/ρ| in the score.")
    parser.add_argument("--weight-muen", type=float, default=0.5, help="Weight for |Δμ_en/ρ| in the score.")
    args = parser.parse_args()

    csv_path = Path(args.csv)
    df = pd.read_csv(csv_path)
    ref_df = load_reference(Path(args.reference))
    df = attach_reference_columns(df, ref_df)
    filtered = filter_rows(df, args.min_transmission, args.max_transmission, args.min_primaries, args.allow_clamped, args.world)
    if filtered.empty:
        raise ValueError("No rows remain after filtering; relax the cuts or rerun the simulation.")

    best = select_best_rows(filtered, args.weight_mu, args.weight_muen)
    if best.empty:
        raise ValueError("Unable to compute best rows (check that delta columns are available).")
    out_path = Path(args.output)
    best.to_csv(out_path, index=False)
    print(f"Saved composite table to {out_path}")

    if args.plot:
        plot_overlay(best, ref_df, Path(args.plot), args)
        print(f"Wrote overlay plot to {args.plot}")


if __name__ == "__main__":
    main()
