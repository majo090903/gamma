#!/usr/bin/env python3
"""Validate that transmission_summary.csv covers all thickness/backing combos used in plots."""

import argparse
from pathlib import Path
from typing import Optional

import pandas as pd


def main() -> None:
    parser = argparse.ArgumentParser(
        description=(
            "Check coverage per (world, thickness, backing) combination and report missing or sparse entries."
        )
    )
    parser.add_argument("--csv", default="transmission_summary.csv", help="Path to the simulation CSV.")
    parser.add_argument(
        "--reference",
        default="nist_reference.csv",
        help="Optional reference CSV; when provided, the script reports how many reference energies were simulated.",
    )
    parser.add_argument(
        "--min-energies",
        type=int,
        default=10,
        help="Minimum number of distinct energies required per (world, thickness, backing).",
    )
    parser.add_argument(
        "--require-sim",
        action="store_true",
        help="Fail if any group has NaN/zero mu_counts_cm2_g values (indicates an incomplete run).",
    )
    parser.add_argument("--output", type=Path, default=None, help="Optional CSV report path.")
    args = parser.parse_args()

    df = pd.read_csv(args.csv)
    required_cols = {"world_half_cm", "thickness_nm", "backing_thickness_um", "E_keV", "mu_counts_cm2_g"}
    missing = required_cols - set(df.columns)
    if missing:
        raise ValueError(f"{args.csv} is missing columns: {missing}")

    ref_energies: Optional[pd.Series] = None
    if args.reference and args.reference.lower() != "none":
        ref_path = Path(args.reference)
        if ref_path.exists():
            ref_df = pd.read_csv(ref_path)
            if "E_keV" in ref_df.columns:
                ref_energies = ref_df["E_keV"].dropna().astype(float).sort_values().reset_index(drop=True)

    def count_reference_overlap(group: pd.DataFrame) -> int:
        if ref_energies is None:
            return 0
        simulated = pd.Series(group["E_keV"].unique())
        overlap = pd.Series(simulated).isin(ref_energies)
        return int(overlap.sum())

    clamp_present = "clamp_flag" in df.columns

    grouped = df.groupby(["world_half_cm", "thickness_nm", "backing_thickness_um"], as_index=False).agg(
        energies=("E_keV", lambda x: len(pd.unique(x.round(6)))),
        E_min=("E_keV", "min"),
        E_max=("E_keV", "max"),
        n_rows=("E_keV", "size"),
        rows_ok=("clamp_flag", lambda x: int((x == 0).sum())) if clamp_present else ("E_keV", "size"),
        rows_clamped=("clamp_flag", lambda x: int((x != 0).sum())) if clamp_present else ("E_keV", lambda x: 0),
        has_nan_mu=("mu_counts_cm2_g", lambda x: int(x.isna().sum())),
    )
    if not clamp_present:
        grouped["rows_ok"] = grouped["n_rows"]
        grouped["rows_clamped"] = 0
    if ref_energies is not None:
        grouped["nist_overlap"] = grouped.apply(
            lambda row: count_reference_overlap(
                df[
                    (df["world_half_cm"] == row["world_half_cm"])
                    & (df["thickness_nm"] == row["thickness_nm"])
                    & (df["backing_thickness_um"] == row["backing_thickness_um"])
                ]
            ),
            axis=1,
        )

    warnings = []
    sparse = grouped[grouped["energies"] < args.min_energies]
    for _, row in sparse.iterrows():
        warnings.append(
            f"[warn] world {row.world_half_cm} cm, thickness {row.thickness_nm} nm, backing "
            f"{row.backing_thickness_um} um has only {row.energies} energy points."
        )

    if args.require_sim:
        bad = grouped[grouped["has_nan_mu"] > 0]
        for _, row in bad.iterrows():
            warnings.append(
                f"[warn] world {row.world_half_cm} cm, thickness {row.thickness_nm} nm, backing "
                f"{row.backing_thickness_um} um contains NaN μ/ρ entries; rerun the macro."
            )

    if warnings:
        for msg in warnings:
            print(msg)
    else:
        print("[info] All groups meet the minimum energy count.")

    columns = [
        "world_half_cm",
        "thickness_nm",
        "backing_thickness_um",
        "energies",
        "E_min",
        "E_max",
        "n_rows",
        "rows_ok",
        "rows_clamped",
        "has_nan_mu",
    ]
    if "nist_overlap" in grouped.columns:
        columns.append("nist_overlap")
    grouped = grouped[columns].sort_values(["world_half_cm", "thickness_nm", "backing_thickness_um"])
    if args.output:
        grouped.to_csv(args.output, index=False)
        print(f"[info] Coverage report written to {args.output}")
    else:
        print(grouped.to_string(index=False))


if __name__ == "__main__":
    main()
