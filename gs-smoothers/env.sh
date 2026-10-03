# Source this file. It sets MACHINE, the GNU make GPU flags, the job launcher
# and the build parallelism. Load your usual modules before sourcing.
#
# Override anything by exporting it first, e.g. LAUNCH="" or NJ=8.

if [[ "${NERSC_HOST:-}" == perlmutter ]]; then
    MACHINE=perlmutter
    # e.g. module load PrgEnv-gnu cudatoolkit craype-accel-nvidia80
    MAKEFLAGS_GPU=${MAKEFLAGS_GPU:-"USE_CUDA=TRUE USE_MPI=TRUE COMP=gnu"}
    LAUNCH=${LAUNCH:-"srun -n 1 -c 32 -G 1"}
elif [[ "${LMOD_SYSTEM_NAME:-}" == frontier || "$(hostname -f 2>/dev/null)" == *frontier* ]]; then
    MACHINE=frontier
    # e.g. module load PrgEnv-amd craype-accel-amd-gfx90a rocm
    MAKEFLAGS_GPU=${MAKEFLAGS_GPU:-"USE_HIP=TRUE USE_MPI=TRUE"}
    LAUNCH=${LAUNCH:-"srun -n 1 -c 8 --gpus=1"}
else
    # Workstation default: one local NVIDIA GPU, no MPI.
    MACHINE=${MACHINE:-$(hostname -s)}
    MAKEFLAGS_GPU=${MAKEFLAGS_GPU:-"USE_CUDA=TRUE CUDA_ARCH=120 USE_MPI=FALSE COMP=gnu"}
    LAUNCH=${LAUNCH:-""}
fi

NJ=${NJ:-16}

export MACHINE MAKEFLAGS_GPU LAUNCH NJ
