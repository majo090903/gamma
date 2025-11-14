#!/usr/bin/env python3

"""
Plot attenuation (mu/rho) and energy-absorption coefficients (mu_en/rho, mu_eff/rho)
versus photon energy for each foil thickness recorded in transmission_summary.csv.
"""

import argparse
from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd


def main():
    parser = argparse.ArgumentParser(
        description="Plot mu/rho and mu_en/rho vs energy for each tungsten foil thickness."
    )
    parser.add_argument(
        "--csv",
        default="transmission_summary.csv",
        help="Path to transmission_summary.csv (default: %(default)s)",
    )
    parser.add_argument(
        "--world",
        type=float,
        default=None,
        help="Optional world half-size (cm) to filter on.",
    )
    parser.add_argument(
        "--output-dir",
        default="plots",
        help="Directory where PNG plots will be saved (default: %(default)s)",
    )
    parser.add_argument(
        "--energy-unit",
        choices=["keV", "MeV"],
        default="keV",
        help="Energy unit for x-axis (default: keV)",
    )
    # NEW: optional x-axis limits
    parser.add_argument(
        "--xlim",
        nargs=2,
        type=float,
        metavar=("XMIN", "XMAX"),
        help="X-axis range in the same unit as --energy-unit (e.g., --xlim 1e-3 1e2 with --energy-unit MeV).",
    )
    parser.add_argument(
        "--reference",
        default="nist_reference.csv",
        help="Optional reference CSV with columns E_keV, mu_over_rho_cm2_g, mu_en_over_rho_cm2_g (default: %(default)s). Use 'none' to skip.",
    )
    parser.add_argument(
        "--show-raw-foil",
        action="store_true",
        help="Overlay mu_en/rho derived directly from foil-only deposition.",
    )
    parser.add_argument(
        "--show-raw-slab",
        action="store_true",
        help="Overlay mu_en/rho derived from foil+backing deposition (when present).",
    )

    args = parser.parse_args()

    csv_path = Path(args.csv)
    if not csv_path.exists():
        raise FileNotFoundError(f"Could not find {csv_path}")

    df = pd.read_csv(csv_path)

    ref_df = None
    if args.reference.lower() != "none":
        ref_path = Path(args.reference)
        if ref_path.exists():
            ref_df = pd.read_csv(ref_path)
            required = {"E_keV", "mu_over_rho_cm2_g", "mu_en_over_rho_cm2_g"}
            if not required.issubset(ref_df.columns):
                raise ValueError(f"Reference CSV missing columns: {required - set(ref_df.columns)}")
            ref_df = ref_df.rename(
                columns={
                    "mu_over_rho_cm2_g": "mu_ref_cm2_g",
                    "mu_en_over_rho_cm2_g": "mu_en_ref_cm2_g",
                }
            ).sort_values("E_keV")
        else:
            print(f"[warn] Reference CSV {ref_path} not found; skipping overlay.")

    required_cols = {
        "world_half_cm",
        "thickness_nm",
        "E_keV",
        "mu_counts_cm2_g",
        "mu_en_cm2_g",
        "mu_eff_cm2_g",
    }
    missing = required_cols.difference(df.columns)
    if missing:
        raise ValueError(f"Missing expected columns in CSV: {missing}")

    if args.world is not None:
        df = df[df["world_half_cm"] == args.world]
    if df.empty:
        raise ValueError(f"No rows found for world_half_cm == {args.world}")

    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    foil_plot_dir = output_dir / "foil_only"
    backed_plot_dir = output_dir / "backed"
    foil_plot_dir.mkdir(parents=True, exist_ok=True)
    backed_plot_dir.mkdir(parents=True, exist_ok=True)
    error_dir = None
    foil_error_dir = None
    backed_error_dir = None
    if ref_df is not None:
        error_dir = output_dir / "errors"
        foil_error_dir = error_dir / "foil_only"
        backed_error_dir = error_dir / "backed"
        foil_error_dir.mkdir(parents=True, exist_ok=True)
        backed_error_dir.mkdir(parents=True, exist_ok=True)

    has_backing = "backing_thickness_um" in df.columns
    group_cols = ["thickness_nm"]
    if has_backing:
        group_cols.append("backing_thickness_um")

    for key, group in df.groupby(group_cols):
        show_reference_label = True
        if has_backing:
            if isinstance(key, tuple):
                thickness, backing_um = key
            else:
                thickness, backing_um = key, 0.0
            thickness = float(thickness)
            backing_um = float(backing_um)
        else:
            if isinstance(key, tuple):
                thickness = float(key[0])
            else:
                thickness = float(key)
            backing_um = None
        group = group.sort_values("E_keV")
        if has_backing and backing_um > 0.0:
            plot_parent = backed_plot_dir
            error_parent = backed_error_dir
        else:
            plot_parent = foil_plot_dir
            error_parent = foil_error_dir

        if ref_df is not None:
            fig, (ax, diff_ax) = plt.subplots(
                nrows=2,
                ncols=1,
                figsize=(8, 7),
                sharex=True,
                gridspec_kw={"height_ratios": [3, 1]},
            )
        else:
            fig, ax = plt.subplots(figsize=(8, 5))
            diff_ax = None

        if args.energy_unit == "MeV":
            energy = group["E_keV"] / 1000.0
            x_label = "Photon energy (MeV)"
        else:
            energy = group["E_keV"]
            x_label = "Photon energy (keV)"

        ax.loglog(energy, group["mu_counts_cm2_g"], label=r"$\mu/\rho$ (counts)")
        ax.loglog(energy, group["mu_en_cm2_g"], linestyle="--", label=r"$\mu_{en}/\rho$ (CPE)")
        ax.loglog(energy, group["mu_eff_cm2_g"], linestyle=":", label=r"$\mu_{\mathrm{eff}}/\rho$")
        if args.show_raw_foil and "mu_en_raw_cm2_g" in group.columns:
            ax.loglog(
                energy,
                group["mu_en_raw_cm2_g"],
                linestyle="-.",
                alpha=0.65,
                label=r"$\mu_{en}/\rho$ (raw foil)",
            )
        if args.show_raw_slab and "mu_en_raw_slab_cm2_g" in group.columns:
            ax.loglog(
                energy,
                group["mu_en_raw_slab_cm2_g"],
                linestyle="--",
                alpha=0.65,
                label=r"$\mu_{en}/\rho$ (raw slab)",
            )

        if ref_df is not None:
            ref_energy = ref_df["E_keV"] / (1000.0 if args.energy_unit == "MeV" else 1.0)
            ax.loglog(
                ref_energy,
                ref_df["mu_ref_cm2_g"],
                linestyle="--",
                color="red",
                linewidth=1.2,
                label=r"NIST $\mu/\rho$" if show_reference_label else "_nolegend_",
            )
            ax.loglog(
                ref_energy,
                ref_df["mu_en_ref_cm2_g"],
                linestyle="--",
                color="red",
                linewidth=1.2,
                alpha=0.6,
                label=r"NIST $\mu_{en}/\rho$" if show_reference_label else "_nolegend_",
            )
            show_reference_label = False
            if error_dir is not None:
                merged = pd.merge(group, ref_df, on="E_keV", how="inner", suffixes=("", "_ref"))
                if not merged.empty:
                    merged = merged.sort_values("E_keV")
                    ref_energy_subset = merged["E_keV"] / (1000.0 if args.energy_unit == "MeV" else 1.0)
                    ax.scatter(
                        ref_energy_subset,
                        merged["mu_ref_cm2_g"],
                        color="red",
                        marker="x",
                        s=28,
                        linewidths=1.0,
                        label="_nolegend_",
                    )
                    ax.scatter(
                        ref_energy_subset,
                        merged["mu_en_ref_cm2_g"],
                        color="red",
                        marker="d",
                        s=20,
                        alpha=0.8,
                        label="_nolegend_",
                    )
                    ax.scatter(
                        ref_energy_subset,
                        merged["mu_counts_cm2_g"],
                        color="black",
                        marker="o",
                        s=18,
                        facecolors="none",
                        label="_nolegend_",
                    )
                    ax.scatter(
                        ref_energy_subset,
                        merged["mu_en_cpe_cm2_g"],
                        color="black",
                        marker="s",
                        s=18,
                        facecolors="none",
                        label="_nolegend_",
                    )

                    delta_counts_denom = merged["mu_ref_cm2_g"].replace(0.0, pd.NA)
                    merged["delta_mu_counts_percent"] = (
                        (merged["mu_counts_cm2_g"] - merged["mu_ref_cm2_g"]) / delta_counts_denom
                    ) * 100.0

                    delta_cpe_denom = merged["mu_en_ref_cm2_g"].replace(0.0, pd.NA)
                    merged["delta_mu_en_cpe_percent"] = (
                        (merged["mu_en_cpe_cm2_g"] - merged["mu_en_ref_cm2_g"]) / delta_cpe_denom
                    ) * 100.0

                    merged["delta_mu_en_raw_percent"] = (
                        (merged["mu_en_raw_cm2_g"] - merged["mu_en_ref_cm2_g"]) / delta_cpe_denom
                    ) * 100.0
                    if "mu_en_raw_slab_cm2_g" in merged.columns:
                        merged["delta_mu_en_raw_slab_percent"] = (
                            (merged["mu_en_raw_slab_cm2_g"] - merged["mu_en_ref_cm2_g"]) / delta_cpe_denom
                        ) * 100.0

                    merged["delta_mu_counts_cm2_g"] = merged["mu_counts_cm2_g"] - merged["mu_ref_cm2_g"]
                    merged["delta_mu_en_cpe_cm2_g"] = merged["mu_en_cpe_cm2_g"] - merged["mu_en_ref_cm2_g"]
                    merged["delta_mu_en_raw_cm2_g"] = merged["mu_en_raw_cm2_g"] - merged["mu_en_ref_cm2_g"]

                    thickness_label = f"{thickness:g}".replace(".", "_")
                    backing_label = ""
                    if has_backing:
                        backing_label_value = f"{backing_um:g}".replace(".", "_")
                        backing_label = f"_backing{backing_label_value}um"
                    if error_parent is None:
                        error_parent = error_dir
                    error_path = error_parent / f"errors_{thickness_label}nm{backing_label}.csv"
                    error_columns = [
                        "world_half_cm",
                        "thickness_nm",
                    ]
                    if has_backing and "backing_thickness_um" in merged.columns:
                        error_columns.append("backing_thickness_um")
                    error_columns.extend([
                        "E_keV",
                        "mu_counts_cm2_g",
                        "mu_ref_cm2_g",
                        "delta_mu_counts_cm2_g",
                        "delta_mu_counts_percent",
                        "mu_en_cpe_cm2_g",
                        "mu_en_ref_cm2_g",
                        "delta_mu_en_cpe_cm2_g",
                        "delta_mu_en_cpe_percent",
                        "mu_en_raw_cm2_g",
                        "delta_mu_en_raw_cm2_g",
                        "delta_mu_en_raw_percent",
                        "mu_eff_cm2_g",
                    ])
                    if "mu_en_raw_slab_cm2_g" in merged.columns:
                        error_columns.extend(
                            [
                                "mu_en_raw_slab_cm2_g",
                                "delta_mu_en_raw_slab_percent",
                            ]
                        )
                    merged[error_columns].to_csv(error_path, index=False)
                    print(f"Saved {error_path}")

                    if diff_ax is not None:
                        energy_diff = merged["E_keV"] / (1000.0 if args.energy_unit == "MeV" else 1.0)
                        diff_ax.semilogx(
                            energy_diff,
                            merged["delta_mu_counts_percent"],
                            marker="o",
                            linestyle="-",
                            label=r"$\Delta \mu/\rho$ (%)",
                        )
                        diff_ax.semilogx(
                            energy_diff,
                            merged["delta_mu_en_cpe_percent"],
                            marker="s",
                            linestyle="--",
                            label=r"$\Delta \mu_{en}/\rho$ CPE (%)",
                        )
                        diff_ax.semilogx(
                            energy_diff,
                            merged["delta_mu_en_raw_percent"],
                            marker="^",
                            linestyle=":",
                            label=r"$\Delta \mu_{en}/\rho$ raw foil (%)",
                        )
                        if "delta_mu_en_raw_slab_percent" in merged.columns:
                            diff_ax.semilogx(
                                energy_diff,
                                merged["delta_mu_en_raw_slab_percent"],
                                marker="v",
                                linestyle=":",
                                label=r"$\Delta \mu_{en}/\rho$ raw slab (%)",
                            )
                        diff_ax.axhline(0.0, color="black", linewidth=0.8)
                        diff_ax.set_ylabel("Δ [%]")
                        diff_ax.grid(True, which="both", linestyle=":", alpha=0.4)
                        diff_ax.legend()

        ax.set_xlabel(x_label)
        ax.set_ylabel(r"Coefficient (cm$^2$/g)")
        title_world = ""
        if args.world is None and "world_half_cm" in group.columns:
            title_world = f", world {group['world_half_cm'].iloc[0]:.0f} cm"
        title = f"W foil {thickness:g} nm"
        if has_backing:
            if backing_um <= 0.0:
                title += " (no backing)"
            else:
                title += f" + backing {backing_um:g} µm"
        title += title_world
        ax.set_title(title)
        ax.grid(True, which="both", linestyle=":", alpha=0.4)
        ax.legend()
        if diff_ax is not None:
            diff_ax.set_xlabel(x_label)

        # Apply user-provided x-axis limits, if any
        if args.xlim:
            ax.set_xlim(args.xlim[0], args.xlim[1])
            if diff_ax is not None:
                diff_ax.set_xlim(args.xlim[0], args.xlim[1])

        suffix = "MeV" if args.energy_unit == "MeV" else "keV"
        thickness_label = f"{thickness:g}".replace(".", "_")
        backing_label = ""
        if has_backing:
            backing_label_value = f"{backing_um:g}".replace(".", "_")
            backing_label = f"_backing{backing_label_value}um"
        outfile = plot_parent / f"mu_vs_energy_{thickness_label}nm{backing_label}_{suffix}.png"
        fig.tight_layout()
        fig.savefig(outfile, dpi=200)
        plt.close(fig)
        print(f"Saved {outfile}")


if __name__ == "__main__":
    main()
