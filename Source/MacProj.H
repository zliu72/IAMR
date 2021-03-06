
#ifndef _MacProj_H_
#define _MacProj_H_

#include <AMReX_BC_TYPES.H>
#include <AMReX_BCRec.H>
#include <AMReX_Amr.H>
#include <AMReX_AmrLevel.H>
#include <AMReX_FluxRegister.H>
#include <Godunov.H>

class MacProj
{
public:
    
    MacProj (amrex::Amr*   parent,
             int    _finest_level,
             amrex::BCRec* _phys_bc, 
             int    /*not used*/ );
    
    ~MacProj ();

    
    void install_level (int       level, 
                        amrex::AmrLevel* level_data);

    void install_anelastic_coefficient (int                    level, 
                                        amrex::Real**            anel_coeff);
    
    void setup (int level);
    void cleanup (int level);
    //
    // The level advance mac projection.
    //
    void mac_project (int             level,
                      amrex::MultiFab*       u_mac,
                      amrex::MultiFab&       S,
                      amrex::Real            dt,
                      amrex::Real            prev_time,
                      const amrex::MultiFab& divu,
                      int             have_divu,
                      bool            increment_vel_register = true);

    void test_umac_periodic (int level, amrex::MultiFab* u_mac);
    //
    // The sync solve.
    //
    void mac_sync_solve (int       level,
                         amrex::Real      dt, 
                         amrex::MultiFab& rho_half,
                         amrex::IntVect&  fine_ratio);

    // this version is for the closed chamber LMC algorithm
    void mac_sync_solve (int       level,
                         amrex::Real      dt, 
                         amrex::MultiFab& rho_half,
                         amrex::IntVect&  fine_ratio,
			 amrex::MultiFab* Rhs_increment,
			 bool      subtract_avg,
			 amrex::Real&     offset);
    //
    // Computing the sync tendency.
    //
    void mac_sync_compute (int                   level,
                           amrex::MultiFab*             u_mac, 
                           amrex::MultiFab&             Vsync,
                           amrex::MultiFab&             Ssync,
                           amrex::MultiFab&             rho_half, 
                           amrex::FluxRegister*         adv_flux_reg,
                           amrex::Array<AdvectionForm>& advectionType,
                           amrex::Real                  prev_time,
                           amrex::Real                  prev_pres_time,
                           amrex::Real                  dt,
                           int                   NUM_STATE,
                           amrex::Real                  be_cn_theta,
                           bool                  modify_reflux_normal_vel,
                           int		         do_mom_diff,
                           const amrex::Array<int>&     increment_sync = amrex::Array<int>(),
			   bool                  update_fluxreg = true);

    void mac_sync_compute (int                   level,
                           amrex::MultiFab&             Sync,
                           int                   comp,
                           int                   s_ind,
                           const amrex::MultiFab*       const* sync_edges,
			   int                   eComp,
                           amrex::MultiFab&             rho_half,
                           amrex::FluxRegister*         adv_flux_reg,
                           amrex::Array<AdvectionForm>& advectionType,
                           bool		         modify_reflux_normal_vel,
                           amrex::Real                  dt,
			   bool                  update_fluxreg = true);

    void scaleArea (int level, amrex::MultiFab* area, amrex::Real** anel_coeff);

    static void Initialize ();
    static void Finalize ();

protected:
    //
    // Test the divergence constraint.
    //
    void check_div_cond (int      level,
                         amrex::MultiFab U_edge[]) const;
    //
    // Boundary conditions.
    //
    void BuildPhiBC (int level);

    void set_outflow_bcs (int level,
                          amrex::MultiFab* mac_phi,
                          const amrex::MultiFab* u_mac, 
                          const amrex::MultiFab& S,
                          const  amrex::MultiFab& divu);
    //
    // Pointers to amr,amrlevel.
    //
    amrex::Amr*             parent;
    amrex::Array<amrex::AmrLevel*> LevelData;
    //
    // Boundary condition objects.
    //
    amrex::BCRec*                phys_bc;
    amrex::Array< amrex::Array<amrex::BCRec> > phi_bcs;
    //
    // MAC sync correction and solution.
    //
    amrex::Array<std::unique_ptr<amrex::MultiFab    > > mac_phi_crse;
    amrex::Array<std::unique_ptr<amrex::FluxRegister> > mac_reg;
    //
    // Anelastic coefficient array.
    //
    amrex::Array<amrex::Real**> anel_coeff;
    //
    // Parameters.
    //
    int        finest_level;
    int        finest_level_allocated;

    static int  verbose;
    static bool  use_cg_solve;
    static int  do_outflow_bcs;
    static amrex::Real mac_tol;
    static amrex::Real mac_abs_tol;
    static amrex::Real mac_sync_tol;
    static int  check_umac_periodicity;
    static int fix_mac_sync_rhs;
};

#endif
