# Gauss-Seidel smoother tests for AMReX PR 5996

Single-GPU A/B tests for the strided red-black smoothers (`ParallelForStrided`,
`ParallelForRedBlack`). Build the same tests from two AMReX trees, run the same
inputs, then check that the solver output is bitwise identical and compare the
MLMG iteration time.

## Quick start

```bash
# load your usual modules first (PrgEnv, cudatoolkit/rocm, craype-accel-*)
source env.sh                       # perlmutter / frontier / workstation defaults

./build.sh /path/to/amrex-at-merge-base base
./build.sh /path/to/amrex-pr-branch    pr

# inside an allocation (salloc); LAUNCH from env.sh prefixes each run with srun
REPEAT=3 ./run.sh base
REPEAT=3 ./run.sh pr
./compare.py base pr                # add --verbose to see diffs
```

Merge base of PR 5996 is `90b614114b`. GNU make is used throughout; the site
make files pick `CUDA_ARCH=80` on Perlmutter and `AMD_ARCH=gfx90a` on Frontier.
Override `MAKEFLAGS_GPU`, `LAUNCH`, `NJ` or `MACHINE` by exporting them before
sourcing `env.sh`.

## Layout

| path | purpose |
|---|---|
| `env.sh` | machine detection, make flags, launcher |
| `build.sh <amrex> <tag> [test...]` | builds `bin/<tag>/<test>`; logs in `logs/<machine>/` |
| `run.sh <tag> [case...]` | runs `cases.txt`; output in `results/<machine>/<tag>/<case>.r<N>.out` |
| `compare.py <tagA> <tagB>` | identical/DIFFERENT per case plus iteration-time table (min over repeats) |
| `cases.txt`, `cases/*.inputs` | case list and inputs |
| `nodal_variants/` | nodal Poisson driver that selects each MLNodeLaplacian smoother kernel |

Each tag builds into its own `tmp_build_dir_<tag>` so two source trees never
share objects. Tests from the AMReX tree are built in place under
`Tests/LinearSolvers/...` and `Tests/Base/ParallelForStrided`.

## Kernel coverage

| case(s) | executable | kernel exercised |
|---|---|---|
| `cell-poisson-*` | ABecLaplacian_C 3D, prob 1 | `mlpoisson_gsrb` |
| `cell-abec-*`, `cell-abec-neumann-mb` | ABecLaplacian_C 3D, prob 2 / 3 | `abec_gsrb` |
| `cell-poisson-jacobi-mb` | ABecLaplacian_C 3D | Jacobi, unaffected reference |
| `cell2d-*` | ABecLaplacian_C 2D | 2D cell kernels |
| `overset3d` | CellOverset 3D | `abec_gsrb_os` |
| `nodal-aa-*` | nodal_variants 3D | `mlndlap_gscolor_aa` |
| `nodal-ha-mb` | nodal_variants 3D | `mlndlap_gscolor_ha` (harmonic average, coarse levels) |
| `nodal-const-mb` | nodal_variants 3D | `mlndlap_gscolor_c` (constant sigma) |
| `nodal-rap-mb` | nodal_variants 3D | `mlndlap_gauss_seidel_sten` (RAP coarsening) |
| `nodal-jacobi-mb` | nodal_variants 3D | Jacobi, unaffected reference |
| `nodal2d-aa`, `nodal2d-rz` | nodal_variants 2D | 2D nodal kernels, Cartesian and RZ |
| `nodetensor3d` | NodeTensorLap 3D | `MLNodeTensorLaplacian` red-black smoother |
| `pfs3d` | Tests/Base/ParallelForStrided | launcher correctness, prints `N cases, M failures` |

Not covered: `mlpoisson_gsrb_os` (Poisson with overset mask) has no standalone
test, and the 2D metric kernel `mlpoisson_gsrb_m` needs an RZ cell-centered
driver. `nodal2d-rz` is for timing only; its error norms are meaningless
because the manufactured solution is Cartesian.

## Box layouts

The `-mb` cases use 256^3 (2D: 2048^2) with 64^3 boxes and a second AMR level
covering the middle half of the domain. `-1box` is a single 256^3 box, which
takes the single-box checkerboard path. `-8box` is eight 128^3 boxes on one
level: the regime where `isFusingCandidate()` used to say no, and where the PR
now forces the fused strided launch for Gauss-Seidel. A regression there on
MI250X is the main thing to look for.

## Reading the results

- `identical` means every residual and error norm line matches between tags.
  Timing and banner lines are stripped before the diff.
- `B/A` below 1 means the second tag is faster. Use `REPEAT=3` or more; the
  table takes the minimum iteration time over repeats.
- The nodal and `nodetensor3d` cases print per-level `max-norm` / `1-norm`
  errors as a sanity check on convergence.
