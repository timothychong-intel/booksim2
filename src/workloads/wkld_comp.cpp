/*
 * workload component implementation
 *
 * C. Beckmann and H. Dogan (c) Intel, 2023
 * 
 */
#include "wkld_comp.hpp"

// type-specific factories, indexed by name:
map<string, WorkloadComponent::FactoryBase*> WorkloadComponent::factories;

// create a new generator/modifier
WorkloadComponent* WorkloadComponent::New(
      const string &         kind,    // what kind of generator or modifier, e.g. "SWM"
      int                    nodes,   // number of nodes
      const vector<string> & options, // additional options, e.g. "randperm" in "SWM(randperm)"
      Configuration const *  config,
      WorkloadComponent *    upstrm)  // upstream component (should be NULL for generators)
{
   return factories.at(kind)->New(nodes, options, config, upstrm);
}
