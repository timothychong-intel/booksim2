/*
 * roi.cpp
 *
 * Region of Interes class 
 * for Booksim scalable workload model 
 *
 *  Created on: Feb 24, 2023
 *      Author: hdogan
*/

#ifndef _ROI_HPP_
#define _ROI_HPP_

#include "config_utils.hpp"
#include "globals.hpp"
#include <unordered_set>

using namespace std;

class RoI {
protected:
  int _pes;
  int _roi_enabled = 0;
  int _roi_begin;
  int _roi_end;
  int _roi_begin_total_count;
  int _roi_end_total_count;
  int _roi_begin_counter = 0;
  int _roi_end_counter = 0;
  
  // <roi_marker, <pe, marker_count>>
  std::map<int, std::map<int, int>> _pe_marker_count_map;
  std::map<int, int> _roi_marker_count_map;
  std::map<int, int> _roi_count;

  Configuration const * const _config;

  void SwmRoI(int roi, int pe);
  void SwmRoIBegin(int pe);
  void SwmRoIEnd(int pe);
  void SwmRoIBegin();
  void SwmRoIEnd();
  bool IsRoI(int pe, int marker);

public:
  
  RoI(Configuration const * const config);
  ~RoI() {}
  
  void Init(int pes);
};


#endif 
