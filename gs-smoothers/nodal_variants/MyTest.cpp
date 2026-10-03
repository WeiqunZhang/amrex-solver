#include "MyTest.H"

#include <AMReX_MLNodeLaplacian.H>
#include <AMReX_ParmParse.H>

#include <numbers>

using namespace amrex;

MyTest::MyTest ()
{
    readParameters();
    initData();
}

void
MyTest::solve ()
{
    BL_PROFILE("NodalVariants::solve()");

    LPInfo info;
    info.setAgglomeration(agglomeration);
    info.setConsolidation(consolidation);
    info.setMaxCoarseningLevel(max_coarsening_level);

    const Real const_sigma = (sigma_type == "const") ? Real(1.0) : Real(0.0);
    MLNodeLaplacian linop(geom, grids, dmap, info, {}, const_sigma);

    linop.setDomainBC({AMREX_D_DECL(LinOpBCType::Dirichlet,
                                    LinOpBCType::Dirichlet,
                                    LinOpBCType::Dirichlet)},
                      {AMREX_D_DECL(LinOpBCType::Dirichlet,
                                    LinOpBCType::Dirichlet,
                                    LinOpBCType::Dirichlet)});

    if (coarsening == "rap") {
        linop.setCoarseningStrategy(MLNodeLaplacian::CoarseningStrategy::RAP);
    }
    if (sigma_type == "ha") {
        linop.setHarmonicAverage(true);
    }
    if (sigma_type != "const") {
        for (int ilev = 0; ilev <= max_level; ++ilev) {
            linop.setSigma(ilev, sigma[ilev]);
        }
    }
    linop.setGaussSeidel(use_gauss_seidel != 0);

    MLMG mlmg(linop);
    mlmg.setMaxIter(max_iter);
    mlmg.setMaxFmgIter(max_fmg_iter);
    mlmg.setVerbose(verbose);
    mlmg.setBottomVerbose(bottom_verbose);

    // Exact solution on the domain boundary, zero inside.
    for (int ilev = 0; ilev <= max_level; ++ilev) {
        MultiFab::Copy(solution[ilev], exact_solution[ilev], 0, 0, 1, 0);
        const Box& interior = amrex::surroundingNodes(
            amrex::grow(geom[ilev].Domain(), -1));
        solution[ilev].setVal(0.0, interior, 0, 1, 0);
    }

    mlmg.solve(GetVecOfPtrs(solution), GetVecOfConstPtrs(rhs), reltol, 0.0);
}

void
MyTest::compute_norms () const
{
    for (int ilev = 0; ilev <= max_level; ++ilev) {
        amrex::Print() << "Level " << ilev << "\n";
        MultiFab error(solution[ilev].boxArray(), solution[ilev].DistributionMap(), 1, 0);
        MultiFab::Copy(error, solution[ilev], 0, 0, 1, 0);
        MultiFab::Subtract(error, exact_solution[ilev], 0, 0, 1, 0);

        auto mask = error.OwnerMask(geom[ilev].periodicity());

        amrex::Print() << "    max-norm: " << error.norm0(*mask, 0, 0) << "\n";
        const Real* dx = geom[ilev].CellSize();
        Real dvol = AMREX_D_TERM(dx[0], *dx[1], *dx[2]);
        amrex::Print() << "    1-norm  : " << error.norm1(0, geom[ilev].periodicity())*dvol << "\n";
    }
}

void
MyTest::readParameters ()
{
    ParmParse pp;
    pp.query("max_level", max_level);
    pp.query("ref_ratio", ref_ratio);
    pp.query("n_cell", n_cell);
    pp.query("max_grid_size", max_grid_size);
    pp.query("coord", coord);

    pp.query("sigma_type", sigma_type);
    pp.query("coarsening", coarsening);
    pp.query("use_gauss_seidel", use_gauss_seidel);

    pp.query("verbose", verbose);
    pp.query("bottom_verbose", bottom_verbose);
    pp.query("max_iter", max_iter);
    pp.query("max_fmg_iter", max_fmg_iter);
    pp.query("reltol", reltol);
    pp.query("agglomeration", agglomeration);
    pp.query("consolidation", consolidation);
    pp.query("max_coarsening_level", max_coarsening_level);
    pp.query("num_trials", num_trials);

    AMREX_ALWAYS_ASSERT_WITH_MESSAGE(sigma_type == "aa" || sigma_type == "ha"
                                     || sigma_type == "const",
                                     "sigma_type must be aa, ha or const");
    AMREX_ALWAYS_ASSERT_WITH_MESSAGE(coarsening == "sigma" || coarsening == "rap",
                                     "coarsening must be sigma or rap");
    AMREX_ALWAYS_ASSERT_WITH_MESSAGE(coord == 0 || (coord == 1 && AMREX_SPACEDIM == 2),
                                     "coord = 1 (RZ) is 2D only");
}

void
MyTest::initData ()
{
    int nlevels = max_level + 1;
    geom.resize(nlevels);
    grids.resize(nlevels);
    dmap.resize(nlevels);

    solution.resize(nlevels);
    rhs.resize(nlevels);
    exact_solution.resize(nlevels);
    sigma.resize(nlevels);

    RealBox rb({AMREX_D_DECL(0.,0.,0.)}, {AMREX_D_DECL(1.,1.,1.)});
    Array<int,AMREX_SPACEDIM> is_periodic{AMREX_D_DECL(0,0,0)};
    Geometry::Setup(&rb, coord, is_periodic.data());
    Box domain0(IntVect{AMREX_D_DECL(0,0,0)}, IntVect{AMREX_D_DECL(n_cell-1,n_cell-1,n_cell-1)});
    Box domain = domain0;
    for (int ilev = 0; ilev < nlevels; ++ilev)
    {
        geom[ilev].define(domain);
        domain.refine(ref_ratio);
    }

    domain = domain0;
    for (int ilev = 0; ilev < nlevels; ++ilev)
    {
        grids[ilev].define(domain);
        grids[ilev].maxSize(max_grid_size);
        domain.grow(-n_cell/4);   // fine level covers the middle of the coarse domain
        domain.refine(ref_ratio);
    }

    for (int ilev = 0; ilev < nlevels; ++ilev)
    {
        dmap[ilev].define(grids[ilev]);
        const BoxArray& nba = amrex::convert(grids[ilev],IntVect::TheNodeVector());
        solution      [ilev].define(nba        , dmap[ilev], 1, 0);
        rhs           [ilev].define(nba        , dmap[ilev], 1, 0);
        exact_solution[ilev].define(nba        , dmap[ilev], 1, 0);
        sigma         [ilev].define(grids[ilev], dmap[ilev], 1, 0);

        const auto dx = geom[ilev].CellSizeArray();

#ifdef AMREX_USE_OMP
#pragma omp parallel if (Gpu::notInLaunchRegion())
#endif
        for (MFIter mfi(rhs[ilev],TilingIfNotGPU()); mfi.isValid(); ++mfi)
        {
            const Box& bx = mfi.tilebox();
            Array4<Real> const phi = exact_solution[ilev].array(mfi);
            Array4<Real> const rh  = rhs[ilev].array(mfi);
            amrex::ParallelFor(bx, [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
            {
                constexpr Real pi = std::numbers::pi_v<Real>;
                constexpr Real tpi = 2.*pi;
                constexpr Real fpi = 4.*pi;
                constexpr Real fac = tpi*tpi*AMREX_SPACEDIM;

                Real x = Real(i)*dx[0];
#if (AMREX_SPACEDIM > 1)
                Real y = Real(j)*dx[1];
#else
                Real y = Real(0.0);
#endif
#if (AMREX_SPACEDIM > 2)
                Real z = Real(k)*dx[2];
#else
                Real z = Real(0.0);
#endif

                phi(i,j,k) = (std::cos(tpi*x) * std::cos(tpi*y) * std::cos(tpi*z))
                    + Real(0.25) * (std::cos(fpi*x) * std::cos(fpi*y) * std::cos(fpi*z));

                rh(i,j,k) = -fac * (std::cos(tpi*x) * std::cos(tpi*y) * std::cos(tpi*z))
                    -        fac * (std::cos(fpi*x) * std::cos(fpi*y) * std::cos(fpi*z));
            });
        }

        sigma[ilev].setVal(1.0);
    }
}
