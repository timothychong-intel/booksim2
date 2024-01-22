/*
 * roi.cpp
 *
 * Region of Interes class 
 * for Booksim scalable workload model 
 *
 *  Created on: Feb 24, 2023
 *      Author: hdogan
 */

#include "roi.hpp"



RoI::RoI(Configuration const * const config) 
   : _config(config) 
{}

void RoI::Init(int pes)
{
   _pes = pes;
   _roi_enabled   = _config->GetInt("roi");
   _roi_begin     = _config->GetInt("roi_begin"); 
   _roi_end       = _config->GetInt("roi_end");

   _roi_begin_total_count = _pes * _config->GetInt("roi_begin_count");
   _roi_end_total_count   = _pes * _config->GetInt("roi_end_count");

   _roi_count[_roi_begin] =  _config->GetInt("roi_begin_count");
   _roi_count[_roi_end] =  _config->GetInt("roi_end_count");

}


// Check if a PE meets the criteria to enter or leave an RoI given a marker
// We compare marker againts roi_begin and roi_end
// and check the marker count
bool RoI::IsRoI(int pe, int marker) {
  if(marker == _roi_begin || marker == _roi_end) {
    return _pe_marker_count_map[marker][pe] == _roi_count[marker]; 
  }
  else
    return false;
}


void RoI::SwmRoIBegin(int pe) {

  // If it first time to arrive RoIBegin for PE, 
  // set the PE roi marker counter to 0
  if(_pe_marker_count_map[_roi_begin].count(pe) == 0)
      _pe_marker_count_map[_roi_begin][pe] = 0; 
   
  // Per PE RoI counter for RoI begin
  // Count how many times this specific 
  // marker is being called by this PE
   _pe_marker_count_map[_roi_begin][pe]++; 

   // RoI counter for RoI begin
   _roi_begin_counter++;


   if(_roi_begin_counter == _roi_begin_total_count) {
      printf("Detailed simulation enabled\n");
      gSimEnabled = true;
   }
}

void RoI::SwmRoIEnd(int pe) {
   
  if(_pe_marker_count_map[_roi_end].count(pe) == 0)
      _pe_marker_count_map[_roi_end][pe] = 0; 
   
   _pe_marker_count_map[_roi_end][pe]++; // Per per RoI counter for RoI begin
   _roi_end_counter++;
   
   if(_roi_end_counter == _roi_end_total_count) {
      printf("Detailed simulation disabled\n");
      gSimEnabled = false;
   }
}


void RoI::SwmRoI(int marker, int pe) {
   
   if(_roi_enabled) {
      if(marker == _roi_begin)
         SwmRoIBegin(pe);
      else if(marker == _roi_end && gSimEnabled)
         SwmRoIEnd(pe);
   }
}
