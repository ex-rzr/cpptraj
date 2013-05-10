#ifndef INC_ACTION_DENSITY_H
#define INC_ACTION_DENSITY_H

#include <vector>
#include <map>

#include "Action.h"
#include "ImagedAction.h"


/** \author Hannes H. Loeffler
  */

class Action_Density : public Action, ImagedAction {
public:
  Action_Density();

  static DispatchObject* Alloc() {
    return (DispatchObject*) new Action_Density();
  }
    
  static void Help();

private:
  Action::RetType Init(ArgList&, TopologyList*, FrameList*, DataSetList*,
		       DataFileList*, int);
  Action::RetType Setup(Topology*, Topology**);
  Action::RetType DoAction(int, Frame*, Frame**);
  void Print();

  static const std::string emptystring;

  enum DirectionType {DX = 0, DY, DZ};
  enum PropertyType {NUMBER = 0, MASS, CHARGE, ELECTRON};

  DirectionType direction_;
  PropertyType property_;

  int frameNum_;
  double delta_;

  CpptrajFile output_;

  std::vector<AtomMask> masks_;
  std::vector<std::map<int,double> > histograms_;  // std::unordered_map may be
						// better but is C++11, some
						// STL's may have hash_map but
						// not same number of params
  std::vector<std::vector<double> > properties_;
};
#endif    
