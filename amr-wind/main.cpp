#include <AMReX.H>
#include <AMReX_MLMG.H>
#include <AMReX_MLPoisson.H>
#include <AMReX_MLNodeABecLaplacian.H>
#include <AMReX_ParmParse.H>

using namespace amrex;

void main_main ()
{
    BL_PROFILE("main");

    int nlevels = 3;
    Vector<Geometry> geom(nlevels);
    Vector<BoxArray> grids(nlevels);
    Vector<DistributionMapping> dmap(nlevels);
    for (int ilev = 0; ilev < nlevels; ++ilev) {
        if (ilev == 0) {
            geom[0].define(Box(IntVect(0), IntVect(1023,767,95)),
                           RealBox({-1095.0, -3845.0, 0.0}, {9145.0, 3835.0, 960.0}),
                           0, Array<int,AMREX_SPACEDIM>{0,1,0});
        } else {
            geom[ilev] = amrex::refine(geom[ilev-1], 2);
        }
        std::ifstream bastream("ba-"+std::to_string(ilev));
        grids[ilev].readFrom(bastream);
        std::ifstream dmstream("dm-"+std::to_string(ilev));
        dmap[ilev].readFrom(dmstream);
        // Fix dmap in case the number of processes is different now.
        auto pmap = dmap[ilev].ProcessorMap();
        int nprocs = ParallelDescriptor::NProcs();
        for (auto& rank : pmap) {
            rank = rank % nprocs;
        }
        dmap[ilev] = DistributionMapping(std::move(pmap));
    }

    Real const reltol = 1.e-6;

    int mac_proj = 1, nodal_proj = 1;
    {
        ParmParse pp;
        pp.query("mac_proj", mac_proj);
        pp.query("nodal_proj", nodal_proj);
    }

    // constant coefficient mac projection
    if (mac_proj)
    {
        Vector<MultiFab> phi(nlevels);
        Vector<MultiFab> rhs(nlevels);
        for (int ilev = 0; ilev < nlevels; ++ilev) {
            phi[ilev].define(grids[ilev], dmap[ilev], 1, 1);
            rhs[ilev].define(grids[ilev], dmap[ilev], 1, 0);
            amrex::FillRandom(rhs[ilev], 0, 1);
        }

        LPInfo info{};
        MLPoisson mlpoisson(geom, grids, dmap, info);
        mlpoisson.setDomainBC({LinOpBCType::Neumann, LinOpBCType::Periodic, LinOpBCType::Neumann},
                              {LinOpBCType::Neumann, LinOpBCType::Periodic, LinOpBCType::Dirichlet});
        for (int ilev = 0; ilev < nlevels; ++ilev) {
            mlpoisson.setLevelBC(ilev, nullptr);
        }

        MLMG mlmg(mlpoisson);
        mlmg.setVerbose(2);
        mlmg.setBottomVerbose(1);

        // warm-up
        for (int ilev = 0; ilev < nlevels; ++ilev) {
            phi[ilev].setVal(0);
        }
        mlmg.solve(GetVecOfPtrs(phi), GetVecOfConstPtrs(rhs), reltol, 0.);
        for (int ilev = 0; ilev < nlevels; ++ilev) {
            phi[ilev].setVal(0);
        }

        mlmg.setVerbose(1);
        mlmg.setBottomVerbose(0);

        {
            BL_PROFILE_REGION("MACPROJ-REG");
            mlmg.solve(GetVecOfPtrs(phi), GetVecOfConstPtrs(rhs), reltol, 0.);
        }
    }

    // constant coefficient nodal projection
    if (nodal_proj)
    {

    }
}

int main (int argc, char* argv[])
{
    {
        ParmParse pp("tiny_profiler");
        pp.add("memprof_enabled", false);
    }
    amrex::Initialize(argc,argv);
    main_main();
    amrex::Finalize();
}
