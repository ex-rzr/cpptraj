// Action_Pucker
#include "Action_Pucker.h"
#include "CpptrajStdio.h"
#include "Constants.h" // RADDEG
#include "TorsionRoutines.h"
#include "DataSet_double.h"

// CONSTRUCTOR
Action_Pucker::Action_Pucker() :
  pucker_(0),
  amplitude_(0),
  puckerMethod_(ALTONA),
  useMass_(true),
  range360_(false),
  offset_(0.0)
{ } 

void Action_Pucker::Help() {
  mprintf("\t[<name>] <mask1> <mask2> <mask3> <mask4> <mask5> [<mask6>] out <filename>\n");
  mprintf("\t[range360] [amplitude] [altona | cremer] [offset <offset>]\n");
  mprintf("\tCalculate pucker of atoms in masks 1-5.\n");
}

// Action_Pucker::init()
Action::RetType Action_Pucker::Init(ArgList& actionArgs, TopologyList* PFL, FrameList* FL,
                          DataSetList* DSL, DataFileList* DFL, int debugIn)
{
  // Get keywords
  DataFile* outfile = DFL->AddDataFile( actionArgs.GetStringKey("out"), actionArgs);
  if      (actionArgs.hasKey("altona")) puckerMethod_=ALTONA;
  else if (actionArgs.hasKey("cremer")) puckerMethod_=CREMER;
  bool calc_amp = actionArgs.hasKey("amplitude");
  offset_ = actionArgs.getKeyDouble("offset",0.0);
  range360_ = actionArgs.hasKey("range360");
  DataSet::scalarType stype = DataSet::UNDEFINED;
  std::string stypename = actionArgs.GetStringKey("type");
  if ( stypename == "pucker" ) stype = DataSet::PUCKER;

  // Get Masks
  Masks_.clear();
  std::string mask_expression = actionArgs.GetMaskNext();
  while (!mask_expression.empty()) {
    Masks_.push_back( AtomMask( mask_expression ) );
    mask_expression = actionArgs.GetMaskNext();
  }
  if (Masks_.size() < 5 || Masks_.size() > 6) {
    mprinterr("Error: Pucker can only be calculated for 5 or 6 masks, %zu specified.\n",
              Masks_.size());
    return Action::ERR;
  }
  // Set up array to hold coordinate vectors.
  AX_.resize( Masks_.size() );

  // Setup dataset
  pucker_ = DSL->AddSet(DataSet::DOUBLE, actionArgs.GetStringNext(),"Pucker");
  if (pucker_ == 0) return Action::ERR;
  pucker_->SetScalar( DataSet::M_PUCKER, stype );
  if (calc_amp)
    amplitude_ = DSL->AddSetAspect(DataSet::DOUBLE, pucker_->Name(), "Amp");
  // Add dataset to datafile list
  if (outfile != 0) {
    outfile->AddSet( pucker_ );
    if (amplitude_ != 0) outfile->AddSet( amplitude_ );
  }

  mprintf("    PUCKER: ");
  for (std::vector<AtomMask>::const_iterator MX = Masks_.begin();
                                             MX != Masks_.end(); ++MX)
  {
    if (MX != Masks_.begin()) mprintf("-");
    mprintf("[%s]", (*MX).MaskString());
  }
  mprintf("\n");
  if (puckerMethod_==ALTONA) 
    mprintf("            Using Altona & Sundaralingam method.\n");
  else if (puckerMethod_==CREMER)
    mprintf("            Using Cremer & Pople method.\n");
  if (outfile != 0) 
    mprintf("            Data will be written to %s\n", outfile->DataFilename().base());
  if (amplitude_!=0)
    mprintf("            Amplitudes will be stored.\n");
  if (offset_!=0)
    mprintf("            Offset: %lf will be added to values.\n");
  if (range360_)
    mprintf("              Output range is 0 to 360 degrees.\n");
  else
    mprintf("              Output range is -180 to 180 degrees.\n");

  return Action::OK;
}

// Action_Pucker::setup
Action::RetType Action_Pucker::Setup(Topology* currentParm, Topology** parmAddress) {
  mprintf("\t");
  for (std::vector<AtomMask>::iterator MX = Masks_.begin();
                                       MX != Masks_.end(); ++MX)
  {
    if ( currentParm->SetupIntegerMask( *MX ) ) return Action::ERR;
    (*MX).BriefMaskInfo();
    if ((*MX).None()) {
      mprintf("\nWarning: pucker: Mask '%s' selects no atoms for topology '%s'\n",
              (*MX).MaskString(), currentParm->c_str());
      return Action::ERR;
    }
  }
  mprintf("\n");

  return Action::OK;  
}

// Action_Pucker::action()
Action::RetType Action_Pucker::DoAction(int frameNum, Frame* currentFrame, Frame** frameAddress) {
  double pval, aval;
  std::vector<Vec3>::iterator ax = AX_.begin(); 

  if (useMass_) {
    for (std::vector<AtomMask>::const_iterator MX = Masks_.begin();
                                               MX != Masks_.end(); ++MX, ++ax)
      *ax = currentFrame->VCenterOfMass( *MX );
  } else {
     for (std::vector<AtomMask>::const_iterator MX = Masks_.begin();
                                               MX != Masks_.end(); ++MX, ++ax)
      *ax = currentFrame->VGeometricCenter( *MX );
  }

  switch (puckerMethod_) {
    case ALTONA: 
      pval = Pucker_AS( AX_[0].Dptr(), AX_[1].Dptr(), AX_[2].Dptr(), AX_[3].Dptr(), AX_[4].Dptr(), aval );
      break;
    case CREMER:
      pval = Pucker_CP( AX_[0].Dptr(), AX_[1].Dptr(), AX_[2].Dptr(), AX_[3].Dptr(), AX_[4].Dptr(), AX_[5].Dptr(), AX_.size(), aval );
      break;
  }
  if ( amplitude_ != 0 )
    amplitude_->Add(frameNum, &aval);
  pval *= RADDEG;
  pucker_->Add(frameNum, &pval);

  return Action::OK;
} 

void Action_Pucker::Print() {
  double puckermin, puckermax;
  if (range360_) {
    puckermax =  360.0;
    puckermin =    0.0;
  } else {
    puckermax =  180.0;
    puckermin = -180.0;
  }
  // Deal with offset and wrap values
  DataSet_double& ds = static_cast<DataSet_double&>( *pucker_ );
  for (DataSet_double::iterator dval = ds.begin(); dval != ds.end(); ++dval) {
    *dval += offset_;
    if ( *dval > puckermax )
      *dval -= 360.0;
    else if ( *dval < puckermin )
      *dval += 360.0;
  }
}
