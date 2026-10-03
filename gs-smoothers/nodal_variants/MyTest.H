#ifndef MY_TEST_H_
#define MY_TEST_H_

#include <AMReX_MLMG.H>

// Nodal Poisson solve that can select each MLNodeLaplacian smoother kernel:
//   sigma_type = aa | ha | const      (arithmetic / harmonic average / constant sigma)
//   coarsening = sigma | rap          (rap uses the stencil kernels on coarse levels)
//   coord      = 0 | 1                (1 = RZ, 2D only; perf only, norms are meaningless)
//   use_gauss_seidel = 1 | 0
class MyTest
{
public:

    MyTest ();

    void solve ();
    void compute_norms () const;

    [[nodiscard]] int getNumTrials () const { return num_trials; }

private:

    void readParameters ();
    void initData ();

    int max_level = 1;
    int ref_ratio = 2;
    int n_cell = 128;
    int max_grid_size = 64;
    int coord = 0;

    std::string sigma_type = "aa";
    std::string coarsening = "sigma";
    int use_gauss_seidel = 1;

    int verbose = 2;
    int bottom_verbose = 0;
    int max_iter = 100;
    int max_fmg_iter = 0;
    amrex::Real reltol = amrex::Real(1.e-11);
    bool agglomeration = true;
    bool consolidation = true;
    int max_coarsening_level = 30;
    int num_trials = 1;

    amrex::Vector<amrex::Geometry> geom;
    amrex::Vector<amrex::BoxArray> grids;
    amrex::Vector<amrex::DistributionMapping> dmap;

    amrex::Vector<amrex::MultiFab> solution;
    amrex::Vector<amrex::MultiFab> rhs;
    amrex::Vector<amrex::MultiFab> exact_solution;
    amrex::Vector<amrex::MultiFab> sigma;
};

#endif
