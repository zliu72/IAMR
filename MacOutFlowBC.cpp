
//
// $Id: MacOutFlowBC.cpp,v 1.17 2001-08-09 22:42:00 marc Exp $
//
#include <winstd.H>

#include "MacOutFlowBC.H"
#include "MACOUTFLOWBC_F.H"
#include "ParmParse.H"

#define DEF_LIMITS(fab,fabdat,fablo,fabhi)   \
const int* fablo = (fab).loVect();           \
const int* fabhi = (fab).hiVect();           \
Real* fabdat = (fab).dataPtr();

#define DEF_CLIMITS(fab,fabdat,fablo,fabhi)  \
const int* fablo = (fab).loVect();           \
const int* fabhi = (fab).hiVect();           \
const Real* fabdat = (fab).dataPtr();

#define DEF_BOX_LIMITS(box,boxlo,boxhi)      \
const int* boxlo = (box).loVect();           \
const int* boxhi = (box).hiVect();

Real    MacOutFlowBC::tol     = 1.0e-10;
Real    MacOutFlowBC::abs_tol = 5.0e-10;

int  MacOutFlowBC_MG::verbose           = 0;
bool MacOutFlowBC_MG::useCGbottomSolver = true;
Real MacOutFlowBC_MG::cg_tol            = 1.0e-2;
Real MacOutFlowBC_MG::cg_abs_tol        = 5.0e-12;
Real MacOutFlowBC_MG::cg_max_jump       = 10.0;
int  MacOutFlowBC_MG::cg_maxiter        = 40;
int  MacOutFlowBC_MG::maxIters          = 40;

MacOutFlowBC::MacOutFlowBC ()
{
    ParmParse pp("macoutflow");

    pp.query("tol",tol);
    pp.query("abs_tol",abs_tol);
    solver = (BL_SPACEDIM == 2) ? BC_BACK : BC_MG;
    int solver_type = -1;
    pp.query("solver_type",solver_type);
    if (solver_type != -1)
        solver = OutFlow_Solver_Type(solver_type);
}

void 
MacOutFlowBC::computeBC (FArrayBox*         velFab,
                         FArrayBox&         divuFab,
                         FArrayBox&         rhoFab,
                         FArrayBox&         phiFab,
                         const Geometry&    geom, 
                         const Orientation& outFace)
{
    const Real* dx     = geom.CellSize();
    const Box&  domain = geom.Domain();
    int         face   = int(outFace);
    const int   outDir = outFace.coordDir();
  
    int isPeriodic[BL_SPACEDIM];
    for (int dir = 0; dir < BL_SPACEDIM; dir++)
        isPeriodic[dir] = geom.isPeriodic(dir);
  
    IntVect loFiltered, hiFiltered;
    int isPeriodicFiltered[BL_SPACEDIM];
    Real dxFiltered[BL_SPACEDIM];
    //
    // Filter out direction we don't care about.
    //
    int ncStripWidth = 1;
    Box origBox = BoxLib::adjCell(domain,outFace,ncStripWidth);
    IntVect lo = origBox.smallEnd();
    IntVect hi = origBox.bigEnd();
    //
    // Rearrange the box, dx, and isPeriodic so that the dimension that is 1
    // is the last dimension.
    //
    int cnt = 0;
    for (int dir = 0; dir < BL_SPACEDIM; dir++)
    {
        if (dir != outDir)
	{
            loFiltered[cnt] = lo[dir];
            hiFiltered[cnt] = hi[dir];
            dxFiltered[cnt] = dx[dir];
            isPeriodicFiltered[cnt] = isPeriodic[dir];
            cnt++;
	}
        else
        {
            loFiltered[BL_SPACEDIM-1] = lo[dir];
            hiFiltered[BL_SPACEDIM-1] = hi[dir];
            dxFiltered[BL_SPACEDIM-1] = dx[dir];
            isPeriodicFiltered[BL_SPACEDIM-1] = isPeriodic[dir];
	}
    }

    Box       faceBox(loFiltered,hiFiltered);
    FArrayBox divuExt(faceBox,1);
    FArrayBox rhoExt(faceBox,1);
    FArrayBox uExt[BL_SPACEDIM-1];
 
    for (int dir=0; dir <BL_SPACEDIM-1; dir++)
    {
        uExt[dir].resize(BoxLib::surroundingNodes(faceBox,dir),1);
    }
  
#if (BL_SPACEDIM == 2)
    //
    // Make cc r (set = 1 if cartesian).
    //
    int R_DIR = 0;
    int Z_DIR = 1;
    int perpDir = (outDir == Z_DIR) ? R_DIR : Z_DIR;
    Box region = BoxLib::adjCell(domain,outFace,1);
    region.shift(outDir, (outFace.faceDir() == Orientation::high) ? -1 : 1);
    int r_lo = region.smallEnd(perpDir);
    int r_hi = region.bigEnd(perpDir);
  
    Array<Real> rcen(region.length(perpDir), 1.0);
    if (CoordSys::IsRZ() && perpDir == R_DIR) 
        geom.GetCellLoc(rcen, region, perpDir);
#else
    Array<Real> rcen;
    int r_lo = 0;
    int r_hi = 0;
#endif
   
    DEF_BOX_LIMITS(origBox,origLo,origHi);
    DEF_LIMITS(divuExt,divuEPtr,divuElo,divuEhi);
    DEF_LIMITS(rhoExt ,rhoEPtr ,rhoElo , rhoEhi );
    DEF_LIMITS(divuFab,divuPtr,divulo, divuhi);
    DEF_LIMITS(rhoFab, rhoPtr, rholo, rhohi);
    DEF_LIMITS(uExt[0],  uE0Ptr, uE0lo, uE0hi);
    DEF_LIMITS(velFab[0],velXPtr,velXlo,velXhi);
    DEF_LIMITS(velFab[1],velYPtr,velYlo,velYhi);
#if (BL_SPACEDIM == 3)
    DEF_LIMITS(uExt[1],  uE1Ptr, uE1lo, uE1hi);
    DEF_LIMITS(velFab[2],velZPtr,velZlo,velZhi);
#endif
    //
    // Extrapolate the velocities, divu, and rho to the outflow edge in
    // the shifted coordinate system (where the last dimension is 1).
    //
    int zeroIt;
    FORT_EXTRAP_MAC(
        ARLIM(velXlo), ARLIM(velXhi), velXPtr,
        ARLIM(velYlo), ARLIM(velYhi), velYPtr,
#if (BL_SPACEDIM == 3)
        ARLIM(velZlo), ARLIM(velZhi), velZPtr,
#endif
        ARLIM(divulo),ARLIM(divuhi),divuPtr,
        ARLIM(rholo), ARLIM(rhohi), rhoPtr,
#if (BL_SPACEDIM == 2)
        &r_lo, &r_hi, rcen.dataPtr(), 
#endif
        ARLIM(uE0lo),ARLIM(uE0hi),uE0Ptr,
#if (BL_SPACEDIM == 3)
        ARLIM(uE1lo),ARLIM(uE1hi),uE1Ptr,
#endif
        ARLIM(divuElo),ARLIM(divuEhi),divuEPtr,
        ARLIM(rhoElo),ARLIM(rhoEhi),rhoEPtr,
        origLo,origHi,&face,&zeroIt);
  
    if (zeroIt)
    {
        //
        // No perturbations, set homogeneous bc.
        //
        phiFab.setVal(0);
    }
    else if (solver == BC_MG)
    {
        FArrayBox phiFiltered(faceBox,1);

        DEF_LIMITS(phiFab,phiFabPtr,phiFab_lo,phiFab_hi);
        DEF_LIMITS(phiFiltered,phiFilteredPtr,phiFiltered_lo,phiFiltered_hi);
        int face = int(outFace);
      
        FORT_MAC_SHIFT_PHI(ARLIM(phiFiltered_lo),ARLIM(phiFiltered_hi),
                           phiFilteredPtr,
                           ARLIM(phiFab_lo),ARLIM(phiFab_hi),phiFabPtr,
                           &face);
        FArrayBox rhs;
        FArrayBox* beta = new FArrayBox[BL_SPACEDIM-1];
      
        computeCoefficients(rhs,beta,uExt,divuExt,rhoExt,rcen,
                            r_lo,r_hi,faceBox,dxFiltered,isPeriodicFiltered);
        //
        // Need phi to have ghost cells.
        //
        Box phiGhostBox = OutFlowBC::SemiGrow(phiFiltered.box(),1,BL_SPACEDIM-1);
        FArrayBox phi(phiGhostBox,1);
        phi.setVal(0);
        phi.copy(phiFiltered);
      
        FArrayBox resid(rhs.box(),1);
      
        MacOutFlowBC_MG mac_mg(faceBox,&phi,&rhs,&resid,beta,
                               dxFiltered,isPeriodicFiltered);
      
        mac_mg.solve(tol,abs_tol,2,2,mac_mg.MaxIters(),mac_mg.Verbose());
      
        DEF_LIMITS(phi,phiPtr,phi_lo,phi_hi);
        DEF_BOX_LIMITS(faceBox,lo,hi);
        //
        // Subtract the average phi.
        //
        FORT_MACSUBTRACTAVGPHI(ARLIM(phi_lo),ARLIM(phi_hi),phiPtr,
#if (BL_SPACEDIM == 2)
                               &r_lo,&r_hi,rcen.dataPtr(),
#endif
                               lo,hi,isPeriodicFiltered);
        //
        // Translate the solution back to the original coordinate system.
        //
        FORT_MAC_RESHIFT_PHI(ARLIM(phiFab_lo),ARLIM(phiFab_hi),phiFabPtr,
                             ARLIM(phi_lo),ARLIM(phi_hi),phiPtr,&face);
      
        delete [] beta;

#if (BL_SPACEDIM == 2)
    }
    else if (solver == BC_BACK)
    {
        solveBackSubstitution(phiFab,divuExt,uExt,rhoExt,rcen,r_lo,r_hi,
                              isPeriodic,dxFiltered,faceBox,outFace);
#endif
    }
    else
    {
        BoxLib::Error("MacOutFlowBC::unknown solver_type");
    }
}

#if (BL_SPACEDIM == 2)
void
MacOutFlowBC::solveBackSubstitution (FArrayBox&         phi,
                                     FArrayBox&         divuExt,
                                     FArrayBox*         uExt,
                                     FArrayBox&         rhoExt,
                                     Array<Real>&       rcen,
                                     int                r_lo,
                                     int                r_hi,
                                     int*               isPeriodicFiltered,
                                     Real*              dxFiltered,
                                     Box&               faceBox,
                                     const Orientation& outFace)
{
    int face   = int(outFace);
    int outDir = outFace.coordDir();
    int length = divuExt.length()[0];

    BL_ASSERT(length == uExt[0].length()[0]-1);
    BL_ASSERT(length == rhoExt.length()[0]);
    BL_ASSERT(length == rcen.size());
  
    DEF_BOX_LIMITS(faceBox,faceLo,faceHi);
    DEF_LIMITS(uExt[0],  uE0Ptr, uE0lo, uE0hi);
    DEF_LIMITS(divuExt,divuEPtr,divuElo,divuEhi);
    DEF_LIMITS(rhoExt ,rhoEPtr ,rhoElo , rhoEhi );
    DEF_LIMITS(phi, phiPtr,philo,phihi);

    FORT_MACPHIBC(ARLIM(uE0lo),ARLIM(uE0hi),uE0Ptr,
                  ARLIM(divuElo),ARLIM(divuEhi),divuEPtr,
                  ARLIM(rhoElo),ARLIM(rhoEhi),rhoEPtr,
                  &r_lo, &r_hi, rcen.dataPtr(), dxFiltered,
                  ARLIM(philo), ARLIM(phihi),phiPtr, 
                  faceLo,faceHi,&face,isPeriodicFiltered);
}
#endif

void 
MacOutFlowBC::computeCoefficients (FArrayBox&   rhs,
                                   FArrayBox*   beta,
                                   FArrayBox*   uExt,
                                   FArrayBox&   divuExt,
                                   FArrayBox&   rhoExt,
                                   Array<Real>& rcen,
                                   int          r_lo,
                                   int          r_hi,
                                   Box&         faceBox,
                                   Real*        dxFiltered,
				   int*         isPeriodicFiltered)
{
    rhs.resize(faceBox,1);
    for (int dir = 0; dir < BL_SPACEDIM-1; dir++)
    {
        beta[dir].resize(BoxLib::surroundingNodes(faceBox,dir),1);
    }

    DEF_BOX_LIMITS(faceBox,faceLo,faceHi);
    DEF_LIMITS(divuExt,divuEPtr,divuElo,divuEhi);
    DEF_LIMITS(rhoExt ,rhoEPtr ,rhoElo , rhoEhi );
    DEF_LIMITS(rhs, rhsPtr, rhslo,rhshi);
    DEF_LIMITS(beta[0], beta0Ptr, beta0lo, beta0hi);
    DEF_LIMITS(uExt[0],  uE0Ptr, uE0lo, uE0hi);
#if (BL_SPACEDIM == 3)	
    DEF_LIMITS(beta[1], beta1Ptr, beta1lo, beta1hi);
    DEF_LIMITS(uExt[1],  uE1Ptr, uE1lo, uE1hi);
#endif


    FORT_COMPUTE_MACCOEFF(ARLIM(rhslo),ARLIM(rhshi),rhsPtr,
                          ARLIM(beta0lo),ARLIM(beta0hi),beta0Ptr,
			
#if (BL_SPACEDIM == 3)
                          ARLIM(beta1lo),ARLIM(beta1hi),beta1Ptr,
#endif
                          ARLIM(uE0lo), ARLIM(uE0hi), uE0Ptr,
#if (BL_SPACEDIM == 3)
                          ARLIM(uE1lo), ARLIM(uE1hi), uE1Ptr,
#endif
                          ARLIM(divuElo),ARLIM(divuEhi), divuEPtr,
                          ARLIM(rhoElo),ARLIM(rhoEhi),rhoEPtr,
#if (BL_SPACEDIM == 2)
                          &r_lo,&r_hi,rcen.dataPtr(),
#endif
                          faceLo,faceHi,
                          dxFiltered,isPeriodicFiltered);
}

MacOutFlowBC_MG::MacOutFlowBC_MG (Box&       Domain,
                                  FArrayBox* Phi,
                                  FArrayBox* Rhs,
                                  FArrayBox* Resid,
                                  FArrayBox* Beta,
                                  Real*      H,
                                  int*       IsPeriodic)
    :
    OutFlowBC_MG(Domain,Phi,Rhs,Resid,Beta,H,IsPeriodic,false)
{
    static int first = true;

    if (first)
    {
        first = false;

        ParmParse pp("mac_mg");

        pp.query("v",verbose);
        int use_cg;
        pp.query("useCGbottomSolver",use_cg);

        useCGbottomSolver = (use_cg > 0) ? true : false;

        pp.query("cg_tol",cg_tol);
        pp.query("cg_abs_tol",cg_abs_tol);
        pp.query("cg_max_jump",cg_max_jump);
        pp.query("cg_maxiter",cg_maxiter);
        pp.query("maxIters",maxIters);
    }

    const IntVect& len = domain.length();

    int min_length = 4;
    bool test_side[BL_SPACEDIM-1];
    for (int dir = 0; dir < BL_SPACEDIM-1; dir++)
        test_side[dir] = (len[dir]&1) != 0 || len[dir] < min_length;

    if (D_TERM(1 && ,test_side[0], || test_side[1]))
    {
        if (useCGbottomSolver)
            cgwork = new FArrayBox(domain,4);
    }
    else
    {
        Real newh[BL_SPACEDIM];
        for (int dir = 0; dir < BL_SPACEDIM; dir++)
            newh[dir] = 2*h[dir];

        Box newdomain = OutFlowBC::SemiCoarsen(domain,2,BL_SPACEDIM-1);
        Box grownBox  = OutFlowBC::SemiGrow(newdomain,1,BL_SPACEDIM-1);
      
        FArrayBox* newphi    = new FArrayBox(grownBox,1);
        FArrayBox* newresid  = new FArrayBox(newdomain,1);
        FArrayBox* newrhs    = new FArrayBox(newdomain,1);
        FArrayBox* newbeta   = new FArrayBox[BL_SPACEDIM-1];
        newphi->setVal(0);
        newresid->setVal(0);

        for (int dir = 0; dir < BL_SPACEDIM-1;dir++)
	{
            newbeta[dir].resize(BoxLib::surroundingNodes(newdomain,dir),1);
            newbeta[dir].setVal(0);
	}
      
        DEF_BOX_LIMITS(domain,dom_lo,dom_hi);
        DEF_BOX_LIMITS(newdomain,new_lo,new_hi);
        DEF_LIMITS(beta[0],beta0Ptr,beta0_lo,beta0_hi);
        DEF_LIMITS(newbeta[0],newbeta0Ptr,newbeta0_lo,newbeta0_hi);
#if (BL_SPACEDIM==3)
        DEF_LIMITS(beta[1],beta1Ptr,beta1_lo,beta1_hi);
        DEF_LIMITS(newbeta[1],newbeta1Ptr,newbeta1_lo,newbeta1_hi);
#endif
      
        FORT_COARSIGMA(beta0Ptr,ARLIM(beta0_lo),ARLIM(beta0_hi),
#if (BL_SPACEDIM == 3)
                       beta1Ptr,ARLIM(beta1_lo),ARLIM(beta1_hi),
#endif
                       newbeta0Ptr,ARLIM(newbeta0_lo),ARLIM(newbeta0_hi),
#if (BL_SPACEDIM == 3)
                       newbeta1Ptr,ARLIM(newbeta1_lo),ARLIM(newbeta1_hi),
#endif
                       dom_lo,dom_hi,new_lo,new_hi);
      
        next = new MacOutFlowBC_MG(newdomain,newphi,newrhs,
                                   newresid,newbeta,newh,isPeriodic);
    }
}

int
MacOutFlowBC_MG::Verbose ()
{
    return verbose;
}

int
MacOutFlowBC_MG::MaxIters ()
{
    return maxIters;
}

MacOutFlowBC_MG::~MacOutFlowBC_MG () {}

Real
MacOutFlowBC_MG::residual ()
{
    Real rnorm;

    DEF_BOX_LIMITS(domain,lo,hi);
    DEF_LIMITS(*rhs,rhsPtr,rhslo,rhshi);
    DEF_LIMITS(*resid,residPtr,residlo,residhi);
    DEF_LIMITS(*phi,phiPtr,philo,phihi);
    DEF_LIMITS(beta[0],beta0Ptr,beta0lo,beta0hi);

#if (BL_SPACEDIM == 3)
    DEF_LIMITS(beta[1],beta1Ptr,beta1lo,beta1hi);
#endif

    FORT_MACRESID(ARLIM(rhslo),ARLIM(rhshi),rhsPtr,
                  ARLIM(beta0lo),ARLIM(beta0hi),beta0Ptr,
#if (BL_SPACEDIM == 3)
                  ARLIM(beta1lo),ARLIM(beta1hi),beta1Ptr,
#endif
                  ARLIM(philo), ARLIM(phihi),phiPtr,
                  ARLIM(residlo),ARLIM(residhi), residPtr,
                  lo,hi,h,isPeriodic,&rnorm);
  
    return rnorm;
}

void 
MacOutFlowBC_MG::step (int nGSRB)
{
    if (cgwork != 0)
    {
        Real resnorm = 0.0;

        FArrayBox dest0(phi->box(),1);

        DEF_BOX_LIMITS(domain,lo,hi);
        DEF_LIMITS(*phi,phiPtr,phi_lo,phi_hi);
        DEF_LIMITS(*resid,residPtr,resid_lo,resid_hi);
        DEF_LIMITS(dest0,dest0Ptr,dest0_lo,dest0_hi);
        DEF_LIMITS(*rhs,rhsPtr,rhs_lo,rhs_hi);
        DEF_LIMITS(beta[0], beta0Ptr, beta0_lo,beta0_hi); 
        DEF_LIMITS(*cgwork,dummPtr,cg_lo,cg_hi);
#if (BL_SPACEDIM == 3)
        DEF_LIMITS(beta[1], beta1Ptr, beta1_lo,beta1_hi);
#endif
    
        FORT_SOLVEMAC(phiPtr, ARLIM(phi_lo),ARLIM(phi_hi),
                      dest0Ptr,ARLIM(dest0_lo),ARLIM(dest0_hi),
                      rhsPtr, ARLIM(rhs_lo),ARLIM(rhs_hi),
                      beta0Ptr, ARLIM(beta0_lo),ARLIM(beta0_hi),
#if (BL_SPACEDIM == 3)
                      beta1Ptr,ARLIM(beta1_lo),ARLIM(beta1_hi),
#endif
                      cgwork->dataPtr(0), ARLIM(cg_lo),ARLIM(cg_hi),
                      cgwork->dataPtr(1), ARLIM(cg_lo),ARLIM(cg_hi),
                      cgwork->dataPtr(2), ARLIM(cg_lo),ARLIM(cg_hi),
                      cgwork->dataPtr(3), ARLIM(cg_lo),ARLIM(cg_hi),
                      residPtr, ARLIM(resid_lo), ARLIM(resid_hi),
                      lo,hi,h,isPeriodic,&cg_maxiter,&cg_tol,
                      &cg_abs_tol,&cg_max_jump,&resnorm);
    }
    else
    {
        gsrb(nGSRB);
    }
}

void 
MacOutFlowBC_MG::restrict ()
{
    DEF_BOX_LIMITS(domain,lo,hi);
    DEF_BOX_LIMITS(next->theDomain(),loc,hic);
    DEF_LIMITS(*resid,residPtr,resid_lo,resid_hi);
    DEF_LIMITS(*(next->theRhs()),rescPtr,resc_lo,resc_hi);

    FORT_RESTRICT(residPtr, ARLIM(resid_lo),ARLIM(resid_hi), 
                  rescPtr, ARLIM(resc_lo),ARLIM(resc_hi), 
                  lo,hi,loc,hic);
}

void 
MacOutFlowBC_MG::interpolate ()
{
    DEF_BOX_LIMITS(domain,lo,hi);
    DEF_BOX_LIMITS(next->theDomain(),loc,hic);
    DEF_LIMITS(*phi,phiPtr,phi_lo,phi_hi);
    DEF_LIMITS(*(next->thePhi()),deltacPtr,deltac_lo,deltac_hi);

    FORT_INTERPOLATE(phiPtr, ARLIM(phi_lo),ARLIM(phi_hi), 
                     deltacPtr,ARLIM(deltac_lo),ARLIM(deltac_hi), 
                     lo,hi,loc,hic);
}

void 
MacOutFlowBC_MG::gsrb (int nstep)
{
    DEF_BOX_LIMITS(domain,lo,hi);
    DEF_LIMITS(*rhs, rhsPtr, rhslo,rhshi);
    DEF_LIMITS(beta[0], beta0Ptr, beta0lo, beta0hi);
    DEF_LIMITS(*phi,phiPtr,philo,phihi);
#if (BL_SPACEDIM == 3)	
    DEF_LIMITS(beta[1], beta1Ptr, beta1lo, beta1hi);
#endif

    FORT_MACRELAX(ARLIM(rhslo),ARLIM(rhshi),rhsPtr,
                  ARLIM(beta0lo),ARLIM(beta0hi),beta0Ptr,
#if (BL_SPACEDIM == 3)
                  ARLIM(beta1lo),ARLIM(beta1hi),beta1Ptr,
#endif
                  ARLIM(philo),ARLIM(phihi),phiPtr,
                  lo,hi,h,isPeriodic,&nstep);
}
