// Image 
#include <cmath> //for floor
#include "Action_Image.h"
#include "DistRoutines.h"
#include "CpptrajStdio.h"

// CONSTRUCTOR
Image::Image() {
  //fprintf(stderr,"Image Con\n");
  ComMask=NULL;
  origin = false;
  center = false;
  ortho = false;
  useMass = true;
  triclinic = OFF;
} 

// DESTRUCTOR
Image::~Image() {
  if (ComMask!=NULL) delete ComMask;
}

/*
 * Image::init()
 * Expected call: image [origin] [center] [triclinic | familiar [com <mask>]] <mask>  
 *   origin: center at 0.0, 0.0, 0.0, otherwise center at box center.
 *   center: Use center of mass for imaging, otherwise use first atom.
 *   triclinic: Force imaging with triclinic code.
 *   familiar: Image with triclinic code and shape into familiar trunc. oct. shape.
 *   com <mask>: If familiar, center based on COM of atoms in mask, otherwise use
 *               origin/box.
 *   <mask>: Only image atoms in <mask>. If no mask given all atoms are imaged.
 * Check order is:
 *    1) Keywords
 *    2) Masks
 */
int Image::init() {
  char *mask1;

  // Get keywords
  origin = A->hasKey("origin");
  center = A->hasKey("center");
  if (A->hasKey("familiar")) triclinic = FAMILIAR;
  if (A->hasKey("triclinic")) triclinic = FORCE;

  // Get Masks
  if (triclinic == FAMILIAR) {
    mask1 = A->getKeyString("com",NULL);
    if (mask1!=NULL) {
      ComMask = new AtomMask();
      ComMask->SetMaskString(mask1);
    }
  }
  mask1 = A->getNextMask();
  Mask1.SetMaskString(mask1);
  
  mprintf("    IMAGE: To");
  if (origin)
    mprintf(" origin");
  else
    mprintf(" box center");
  mprintf(" based on");
  if (center)
    mprintf(" center of mass");
  else
    mprintf(" first atom position");
  mprintf(" using atoms in mask %s\n",Mask1.maskString);
  if (triclinic == FORCE)
    mprintf( "           Triclinic On.\n");
  else if (triclinic == FAMILIAR) {
    mprintf( "           Triclinic On, familiar shape");
    if (ComMask!=NULL) 
      mprintf( " centering on atoms in mask %s", ComMask->maskString);
    mprintf(".\n");
  }

  return 0;
}

/*
 * Image::setup()
 * Set Imaging up for this parmtop. Get masks etc.
 * P is set in Action::Setup
 */
int Image::setup() {

  if ( Mask1.SetupCharMask(P,debug) ) return 1;
  if (Mask1.None()) {
    mprintf("    Error: Image::setup: Mask contains 0 atoms.\n");
    return 1;
  }

  useMass = true;
  if (center && P->mass==NULL) {
    mprintf("    Warning: Image::setup: center: Parm %s contains no mass info.\n",
            P->parmName);
    mprintf("             Geometric center will be used instead.\n");
    useMass = false;
  }

  if (P->ifbox==0) {
    mprintf("    Error: Image::setup: Parm %s does not contain box information.\n",
            P->parmName);
    return 1;
  }

  ortho = false;  
  if (P->ifbox==1 && triclinic==OFF) ortho=true;

  if (triclinic == FAMILIAR) {
    if (ComMask!=NULL) {
      if ( ComMask->SetupMask(P,debug) ) return 1;
      if (ComMask->None()) {
        mprintf("    Error: Image::setup: Mask for 'familiar com' contains no atoms.\n");
        return 1;
      }
    }
  }

  return 0;  
}

/*
 * Image::action()
 */
int Image::action() {
  // Orthorhombic
  double bp[3];
  double bm[3];
  // Non-orthorhombic
  double ucell[9];
  double recip[9];
  double fc[3], ffc[3];
  //   Familiar
  double fcom[3];
  int ixyz[3];
  // General
  int begin, end, count;
  int firstAtom, lastAtom, Atom;
  double boxTrans[3];
  double Coord[3];

  // Set up information for orthorhombic cell
  if (ortho) {
    if ( origin ) {
      bp[0] = F->box[0] / 2.0;
      bp[1] = F->box[1] / 2.0;
      bp[2] = F->box[2] / 2.0;
      bm[0] = -bp[0]; 
      bm[1] = -bp[1];
      bm[2] = -bp[2]; 
    } else {
      bp[0] = F->box[0];
      bp[1] = F->box[1];
      bp[2] = F->box[2];
      bm[0] = 0.0;
      bm[1] = 0.0; 
      bm[2] = 0.0; 
    }

  // Set up information for non-orthorhombic cell
  } else {
    // NOTE: Does this need to be done every time?
    F->BoxToRecip(ucell, recip);
    // Set up centering if putting nonortho cell into familiar trunc. oct. shape
    if (triclinic == FAMILIAR) {
      // Use center of mask of atoms in mask
      if (ComMask!=NULL) {
        F->COM(ComMask, fcom, useMass);
      // Use origin
      } else if (origin) {
        fcom[0]=0.0;
        fcom[1]=0.0;
        fcom[2]=0.0;
      // Use box center
      } else {
        fcom[0]=F->box[0] / 2.0; 
        fcom[1]=F->box[1] / 2.0; 
        fcom[2]=F->box[2] / 2.0;
      }
      //fprintf(stdout,"DEBUG: fcom = %lf %lf %lf\n",fcom[0],fcom[1],fcom[2]);
    } 
  }

  begin = 0;

  // Loop over molecules
  firstAtom = 0;
  lastAtom = 0;
  end = P->molecules;
  if (debug>2) 
    mprintf("IMAGE: molecules is %i\n", P->molecules); 

  for (count = begin; count < end; count++) {

    // Molecules
    firstAtom = lastAtom;
    lastAtom = firstAtom + P->atomsPerMol[count];

    if (debug>2)
      mprintf( "  IMAGE processing atoms %i to %i\n", firstAtom+1, lastAtom); 

    // boxTrans will hold calculated translation needed to move atoms back into box
    boxTrans[0] = 0.0;
    boxTrans[1] = 0.0;
    boxTrans[2] = 0.0;

    // Set up position based on first atom or center of mass
    if (center) 
      F->COM(Coord,useMass,firstAtom,lastAtom);
    else
      F->SetCoord(Coord,firstAtom);

    // ORTHORHOMBIC
    if (ortho) {
      // Determine how far coords are out of box
      for (int i=0; i<3; i++) {
        while (Coord[i] < bm[i]) {
          Coord[i] += F->box[i];
          boxTrans[i] += F->box[i];
        }
        while (Coord[i] > bp[i]) {
          Coord[i] -= F->box[i];
          boxTrans[i] -= F->box[i];
        }
      }

    // NON-ORTHORHOMBIC    
    } else {
      fc[0]=(Coord[0]*recip[0]) + (Coord[1]*recip[1]) + (Coord[2]*recip[2]);
      fc[1]=(Coord[0]*recip[3]) + (Coord[1]*recip[4]) + (Coord[2]*recip[5]);
      fc[2]=(Coord[0]*recip[6]) + (Coord[1]*recip[7]) + (Coord[2]*recip[8]);

      if ( origin ) {
        fc[0] += 0.5;
        fc[1] += 0.5;
        fc[2] += 0.5;
      }

      ffc[0] = floor(fc[0]);
      ffc[1] = floor(fc[1]);
      ffc[2] = floor(fc[2]);

      boxTrans[0] -= (ffc[0]*ucell[0] + ffc[1]*ucell[3] + ffc[2]*ucell[6]);
      boxTrans[1] -= (ffc[0]*ucell[1] + ffc[1]*ucell[4] + ffc[2]*ucell[7]);
      boxTrans[2] -= (ffc[0]*ucell[2] + ffc[1]*ucell[5] + ffc[2]*ucell[8]);

      // Put into familiar trunc. oct. shape
      if (triclinic == FAMILIAR) {
        Coord[0] += boxTrans[0];
        Coord[1] += boxTrans[1];
        Coord[2] += boxTrans[2];
        MinImageNonOrtho2(Coord, fcom, F->box, origin, ixyz, ucell, recip);
        if (ixyz[0] != 0 || ixyz[1] != 0 || ixyz[2] != 0) {
          boxTrans[0] += (ixyz[0]*ucell[0] + ixyz[1]*ucell[3] + ixyz[2]*ucell[6]);
          boxTrans[1] += (ixyz[0]*ucell[1] + ixyz[1]*ucell[4] + ixyz[2]*ucell[7]);
          boxTrans[2] += (ixyz[0]*ucell[2] + ixyz[1]*ucell[5] + ixyz[2]*ucell[8]);

          if (debug > 2)
            mprintf( "  IMAGING, FAMILIAR OFFSETS ARE %i %i %i\n", 
                    ixyz[0], ixyz[1], ixyz[2]);
        }
      }  
    }    
   
    //fprintf(stdout,"DEBUG: BoxTrans: %lf %lf %lf\n",boxTrans[0],boxTrans[1],boxTrans[2]);

    // Translate atoms back into the box
    // NOTE: Need to incorporate MASK!
    for (Atom = firstAtom; Atom < lastAtom; Atom++) {
      if (Mask1.AtomInCharMask(Atom))
        F->Translate(boxTrans, Atom);
    }

  } // END loop over count

  return 0;
} 


