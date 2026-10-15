# Mathematical Foundations of Poisoning Attacks on Linear Regression over Cumulative Distribution Functions <!-- omit from toc -->

This repository contains the artifact for the paper [*Mathematical Foundations of Poisoning Attacks on Linear Regression over Cumulative Distribution Functions*](https://dl.acm.org/doi/10.1145/3802085) (SIGMOD 2026).
It provides C++ implementations of the proposed poisoning and upper-bound algorithms, experiment scripts, and plotting code used to generate the experimental figures and a table in the paper.
It also includes scripts to recompile the paper PDF.

## Table of Contents <!-- omit from toc -->

- [Environment](#environment)
  - [Hardware](#hardware)
  - [Software](#software)
- [Getting Started](#getting-started)
  - [Obtain the Code](#obtain-the-code)
  - [Build and Run](#build-and-run)
- [Experiments](#experiments)
  - [Input Data](#input-data)
  - [Setup](#setup)
  - [Running](#running)
  - [Outputs and Completion Checks](#outputs-and-completion-checks)
  - [Clean-up and Reruns](#clean-up-and-reruns)
- [Compare Against Reference](#compare-against-reference)
  - [1. Download reference archives](#1-download-reference-archives)
  - [2. Run comparison](#2-run-comparison)
  - [3. Read the reports](#3-read-the-reports)
  - [What is compared](#what-is-compared)
- [Graphs and Plots](#graphs-and-plots)
  - [Generate the Paper Figures and Table](#generate-the-paper-figures-and-table)
  - [Mapping to Paper Figures and Table](#mapping-to-paper-figures-and-table)
- [Citation](#citation)
- [Acknowledgments](#acknowledgments)

## Environment

### Hardware

We have verified the artifact in the following three environments.
The experimental results reported in the paper were obtained on the ELSA VELUGA (MS-16V4).

| Machine / environment | CPU | Memory | Storage |
|---|---|---|---|
| ELSA VELUGA (MS-16V4) | Intel Core i9-11900H @ 2.50 GHz | 64 GB DDR4-3200 | 1 TB NVMe SSD |
| Apple MacBook Air (M1, 2020) | Apple M1, 8 cores (4P+4E) | 16 GB | ~1 TB SSD |
| AWS EC2 m6i.large | x86_64, 2 vCPUs | 8 GiB | 100 GiB EBS (gp3) |

**Recommended resources for full reproduction (`--all`):**

- CPU: modern multi-core x86_64 or Apple Silicon
- Memory: **≥ 8 GB** host RAM
- Disk: **≥ 50 GB** free space

The run scripts (`docker_run_all.sh` and `docker_run.sh`) pin experiments to **CPU 0** via `--cpuset-cpus=0` and `--cpus=1` for stable timing, and allocate **4 GB** of memory to the Docker container.

### Software

Docker is the recommended way to run the artifact. On the host, you only need Git and a working Docker installation; all experiment dependencies are installed automatically when building the Docker image.

**Host requirements**

| Component | Requirement |
|---|---|
| OS | Linux (x86_64) or macOS (Apple Silicon) |
| Git | Required to clone the repository |
| Docker | Docker Engine on Linux or Docker Desktop on macOS |

We tested the artifact with Docker versions 28.0.4 on ELSA, 24.0.6 on the MacBook Air, and 29.8.2 on AWS EC2.

**Container environment**

The Docker image is based on `ubuntu:22.04` and includes the following dependencies. You do not need to install these on the host.

| Component | Dependencies |
|---|---|
| C++ | `build-essential`, CMake ≥ 3.10, C++17 |
| Libraries | Boost ≥ 1.74, `nlohmann-json3-dev` |
| Python | Python 3, `pandas>=1.5`, `matplotlib>=3.6`, `numpy>=1.21` |
| Paper compilation | TeX Live packages and `latexmk` |

See `Dockerfile` and `requirements.txt` for the dependency installation details.

## Getting Started

### Obtain the Code

Clone the repository:

```bash
git clone https://github.com/atsukisato/PoisoningAttacksOnLinearRegressionOverCDF.git
cd PoisoningAttacksOnLinearRegressionOverCDF
```

### Build and Run

From the repository root, build the Docker image:

```bash
./docker_build.sh
```

Then run the reproduction pipeline. Each mode automatically downloads the datasets (if missing), compiles the C++ tools, generates the experimental datasets, runs the experiments, generates the figures and a table, and compiles the paper PDF.

We recommend checking the pipeline in this order: `--quick`, then `--all-one-seed`, then a full `--all` run.

- **`--quick`**: runs a reduced parameter grid with a single seed to check that the pipeline works.
- **`--all-one-seed`**: runs the full parameter grids with a single seed to check paper-scale behavior before running all 100 seeds.
- **`--all`**: runs the full paper-scale reproduction with seeds `0..99`.

```bash
./docker_run_all.sh --quick
```

```bash
./docker_run_all.sh --all-one-seed
```

```bash
./docker_run_all.sh --all
```

> [!IMPORTANT]
> A full run (`--all`) can take a long time, typically **several days**, depending on your hardware.
> We recommend first checking the pipeline with `--quick`, then `--all-one-seed`, before a full `--all` run.
>
> Approximate wall-clock times are listed below. Docker image build time is excluded, and dataset download time is listed separately.
>
> | Stage / mode | Typical time |
> |---|---|
> | Dataset download (first run only) | **tens of minutes** |
> | `--quick` | **tens of minutes** |
> | `--all-one-seed` | **a few hours** |
> | `--all` | **several days** |

Each command starts a detached container and prints its container ID (`CID`).
Follow progress with:

```bash
tail -f results/run_all.log
```

See [Outputs and Completion Checks](#outputs-and-completion-checks)
for output locations and how to verify successful completion (exit code 0).

## Experiments

### Input Data

We use three real-world datasets distributed through the [SOSD benchmark](https://github.com/learnedsystems/SOSD): Books, Facebook, and OSM.
These datasets are downloaded automatically by `scripts/run_all.sh` when missing.
The following files are required:

- `data/raw/books_200M_uint64`
- `data/raw/fb_200M_uint64`
- `data/raw/osm_cellids_200M_uint64`

Synthetic datasets (`uniform`, `normal`, `exponential`) are generated by `step2_generate_datasets.sh` and do not require a separate download.
Per-experiment legitimate and poisoned arrays are written under `data/generated/`; see [Outputs and Completion Checks](#outputs-and-completion-checks).

### Setup

Parameter grids and seeds are defined in `scripts/env.sh`.

| Mode | Flag | Purpose |
|---|---|---|
| Quick | `--quick` | Sanity-check / laptop-scale sweep (fewer `%`, `n`, `R`; seed `{0}`) |
| All (one seed) | `--all-one-seed` | Paper-scale grids with a single seed (`one_seed_seeds`, currently `{0}`) |
| All | `--all` | Full paper-scale reproduction (seeds `0..99`) |

Notes:

- Loss and attack outputs are deterministic for a fixed seed and input set.
- Absolute running times and lookup times vary by hardware; qualitative trends (e.g., greedy vs upper-bound scaling) should be preserved.
- Prefer `--quick` for functionality checks, `--all-one-seed` for paper-scale trends, and `--all` for full statistical reproduction.
- Running in the recommended order `--quick` → `--all-one-seed` → `--all` is safe: existing JSON/binary outputs are skipped and reused, and only missing work is computed.

### Running

`docker_run_all.sh` executes
`scripts/run_all.sh`,
which runs the full pipeline:

0. Dataset download (`download.sh`; skipped if the three required files already exist under `data/raw/`)
1. Build (`step1_build.sh`)
2. Dataset generation (`step2_generate_datasets.sh`)
3. Poison injection for all attack variants (`step3*.sh`)
4. Loss and query-time measurement (`step4*.sh`)
5. Upper bounds and small-scale optima (`step5*.sh`)
6. Plot and table generation (`step6_generate_plots.sh`)
7. Paper PDF compilation (`step7_compile_paper.sh`)
8. (Optional) Reference comparison (`step8_compare_reference.sh`; only with `--compare-reference`; see [Compare Against Reference](#compare-against-reference))

### Outputs and Completion Checks

Primary outputs:

| Path | Contents |
|---|---|
| `results/run_all.log` | Full pipeline log (`docker_run_all.sh`) |
| `results/system_info.log` | Host/container info captured at start |
| `results/**/*.json` | Per-instance measurement results |
| `results/figures/**/*.pdf` | Generated experiment figures |
| `results/tables/overview.tex` | Generated Table 1 |
| `paper/main.pdf` | Compiled paper (after Step 7) |
| `data/generated/{dataset}/n{n}/[R{R}/]seed{seed}/[lambda{λ}/]{method}_uint64` | Generated legitimate / poisoned key arrays |
| `comparison/` | Reference comparison reports (after `--compare-reference` or Step 8; see [Compare Against Reference](#compare-against-reference)) |

Paths under `data/generated/` encode the dataset, sample size, seed, and method;
an optional domain size `R` (synthetic data) and an optional poisoning budget `lambda` (poisoned arrays) may also appear. Examples:

```text
data/generated/books_200M/n1000/seed0/legitimate_uint64
data/generated/uniform/n1000/R100000/seed0/legitimate_uint64
data/generated/books_200M/n1000/seed0/lambda100/greedy_uint64
```

Attack method names in filenames:

| Attack | Generated filename under the budget directory |
|---|---|
| Greedy | `greedy_uint64` |
| Greedy, duplicates allowed | `greedy_duplicate_allowed_uint64` |
| seg+e | `consecutive_w_endpoints_uint64` |
| seg+e, duplicates allowed | `consecutive_w_endpoints_duplicate_allowed_uint64` |
| seg+e heuristic | `consecutive_w_endpoints_using_relaxed_solution_uint64` |
| Random | `random_uint64` |

Successful completion ends with a message like:

```text
Comprehensive experiment completed!
...
Experiment completed successfully!
```

Success is determined by the container exit code being **0**.

```bash
# assign the container ID printed by docker_run_all.sh
CID="PASTE_CONTAINER_ID_HERE"

# primary check: Status should be "exited" and ExitCode should be 0
docker inspect -f '{{.State.Status}} {{.State.ExitCode}}' "$CID"

# secondary checks: log message and required outputs
tail -n 30 results/run_all.log
ls results/figures/*/*_all.pdf
ls results/tables/overview.tex
ls paper/main.pdf
```

Individual experiment steps skip existing JSON/binary outputs when already present, so partial runs can be resumed safely.

### Clean-up and Reruns

To force a full recomputation, remove the generated data and/or results before rerunning:

```bash
# generated subsets and result JSONs / figures / tables
rm -rf data/generated
rm -rf results

# optional: rebuild from scratch
rm -rf build
```

Raw datasets in `data/raw/` are retained for reuse.
Do not delete them unless you intend to download them again.

To rerun only plots, the table, and paper compilation after results exist, enter the container and run Steps 6–7 (see [Generate the Paper Figures and Table](#generate-the-paper-figures-and-table)).

## Compare Against Reference

You can compare a local run against archived reference data and results published on Zenodo ([doi:10.5281/zenodo.23286272](https://doi.org/10.5281/zenodo.23286272)).

### 1. Download reference archives

From the Zenodo record, download these two ZIP files:

- `reference-data-generated.zip`
- `reference-results.zip`

Place them under `reference/archives/` at the repository root:

```text
reference/archives/SHA256SUMS.txt                 # already in the repository
reference/archives/reference-data-generated.zip   # from Zenodo
reference/archives/reference-results.zip          # from Zenodo
```

### 2. Run comparison

**Option A: run the pipeline with comparison enabled** (verifies and extracts archives, runs Steps 0–7, then compares):

```bash
./docker_run_all.sh --quick --compare-reference
# or: --all-one-seed / --all together with --compare-reference
```

If the archives are missing, the command fails; comparison is not silently skipped.

**Option B: experiments already finished** (prepare and compare only).
From the repository root on the host, open an interactive container:

```bash
./docker_run.sh
```

Then inside the container (`WORKDIR` is `/workspace`):

```bash
./scripts/prepare_reference.sh
./scripts/step8_compare_reference.sh
```

### 3. Read the reports

Reports are written to `comparison/`:

- `summary.txt`
- `mismatch_file_paths.txt`
- `source_only_file_paths.txt`
- `reference_only_file_paths.txt`

### What is compared

The comparison focuses on generated datasets and loss-related results rather than execution times, because query time and poison generation time can vary substantially across environments.

Comparison is file-by-file over relative paths in two trees:

| Local | Reference | Match criterion |
|---|---|---|
| `data/generated/` | `reference/data/generated/` | Exact bytes (same size and SHA-256) |
| `results/` | `reference/results/` | Selected JSON fields (see below) |

For `results/`, only these families are compared (`query_time*` and other paths are skipped):

- `inject_poison*`: JSON field `mse`
- `loss*`: entire JSON object
- `optimal_poison*`: JSON field `loss`
- `upper_bound*`: JSON field `mse_upper_bound`

Numeric values use `math.isclose` with `rtol=1e-6` and `atol=1e-12`. Integers must match exactly; booleans, strings, and object key sets must match with matching types.

> [!NOTE]
> Exact agreement is not always expected. Floating-point differences can slightly change computed losses and alter poison selection when candidates have nearly equal losses. In greedy poisoning, these differences can propagate through successive steps, producing substantially different final poisoned datasets. A data mismatch alone therefore does not imply a reproduction failure; assess it alongside the corresponding loss values and overall experimental trends.

## Graphs and Plots

### Generate the Paper Figures and Table

Plots and Table 1 are generated by `step6_generate_plots.sh`, which runs the Python scripts in `plot/`.
This step is included in `run_all.sh`; it can also be run standalone after measurements finish.

From the repository root on the host, open an interactive container:

```bash
./docker_run.sh
```

Then inside the container (`WORKDIR` is `/workspace`):

```bash
./scripts/step6_generate_plots.sh
./scripts/step7_compile_paper.sh
```

Figure PDFs are written to `results/figures/`; Table 1 is written to `results/tables/overview.tex`.
`step7_compile_paper.sh` copies the required PDFs into `paper/fig/experiments/`, copies `overview.tex` into `paper/table/`, and compiles `main.tex`.

### Mapping to Paper Figures and Table

Experimental figures and Table 1 in the paper are produced from the following generated files.
Figure and table numbers match the published PDF ([doi:10.1145/3802085](https://dl.acm.org/doi/10.1145/3802085));
labels in parentheses refer to `paper/main.tex`.

| Paper figure / table | Generated file |
|---|---|
| Table 1 (`tab:seg_e`) | `results/tables/overview.tex` |
| Figure 6 (`fig:upper_bound_vs_legitimate_seed0`) | `results/figures/lambda_Ls_seed0/seed0_all.pdf` |
| Figure 7 (`fig:upper_bound_vs_greedy`) | `results/figures/lambda_LgrLub/lambda_Lgr_divided_by_Lub_all.pdf` |
| Figure 8 (`fig:greedy_vs_sege`) | `results/figures/lambda_LgrLconsec/lambda_Lgr_divided_by_Lconsec_all.pdf` |
| Figure 9 (`fig:sege_h_vs_sege`) | `results/figures/lambda_Lconsec_using_relaxed_solution_Lconsec/lambda_Lconsec_using_relaxed_solution_divided_by_Lconsec_all.pdf` |
| Figure 10 (`fig:runningtime_percentage`) | `results/figures/lambda_time/lambda_time_all.pdf` |
| Figure 11 (`fig:runningtime_n`) | `results/figures/n_time/n_time_all.pdf` |
| Figure 12 (`fig:ablation_n`) | `results/figures/n_LgrLub/n_LgrLub_all.pdf` |
| Figure 13 (`fig:ablation_R`) | `results/figures/R_LgrLub/R_LgrLub_all.pdf` |
| Figure 14 (`fig:brute_force_vs_greedy`) | `results/figures/lambda_L_ratios_brute_force/lambda_L_ratios_brute_force_all.pdf` |
| Figure 15 (`fig:brute_force_vs_greedy_R`) | `results/figures/R_L_ratios_brute_force/R_L_ratios_brute_force_all.pdf` |
| Figure 17 (`fig:lookuptime_percentage`) | `results/figures/lambda_query_time/lambda_query_time_all.pdf` |

Figures 1–5 and Figure 16 are schematic or explanatory figures and are **not** regenerated by this artifact’s experiment pipeline (hence they are omitted from the table above).
All experimental figures and Table 1 are regenerated by the pipeline.

## Citation

If you use this code, please cite our paper:

```bibtex
@article{sato2026poisoning,
  author = {Sato, Atsuki and Aum{\"u}ller, Martin and Matsui, Yusuke},
  title = {Mathematical Foundations of Poisoning Attacks on Linear Regression over Cumulative Distribution Functions},
  journal = {Proc. ACM Manag. Data},
  volume = {4},
  number = {3},
  numpages = {25},
  year = {2026}
}
```

Paper: [https://dl.acm.org/doi/10.1145/3802085](https://dl.acm.org/doi/10.1145/3802085)

## Acknowledgments

The real-world datasets and the original dataset download script are provided by the [SOSD benchmark](https://github.com/learnedsystems/SOSD).
Please refer to the SOSD repository for dataset sources and associated references.
