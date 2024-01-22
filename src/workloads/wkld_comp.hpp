/*
 * workload component
 *
 * C. Beckmann and H. Dogan (c) Intel, 2023
 *
 */
#pragma once

#include <assert.h>
#include <vector>
#include <string>
#include <swm.hpp>
#include "globals.hpp"
#include "wkld_msg.hpp"
#include "swm_globals.hpp"
#include "roi.hpp"

using namespace std;

typedef WorkloadMessage::msg_t msg_t;
//
// workload component.
// Base class for traffic generators or modifiers
//
class WorkloadComponent
{
      // get() may be called multiple times per cycle, _get_new() only once. Derived classes override
      vector<WorkloadMessagePtr> _last_get;
      virtual WorkloadMessagePtr _get_new(int src) { assert(false); return 0; }

   public:
      WorkloadComponent() {}
      virtual bool test(int src) = 0;
      virtual WorkloadMessagePtr get(int src) { return _last_get[src] ? _last_get[src] : (_last_get[src] = _get_new(src)); }
      virtual void next(int src) { _last_get[src] = 0; }; // derived class next() must call this unless overriding get()
      virtual void eject(WorkloadMessagePtr m) {};

      virtual void Init(int pes, Configuration const *) { _last_get.resize(pes, 0); };
      virtual void FunctionalSim() {};
      static WorkloadComponent* New(const string &, int, const vector<string> &, Configuration const *, WorkloadComponent *);
      virtual ~WorkloadComponent() = default;

   protected:
      class FactoryBase;
      static map<string, FactoryBase*> factories;
      class FactoryBase {
         public:
            FactoryBase(const string &kind) {
               WorkloadComponent::factories.insert({kind, this});
            }
            virtual WorkloadComponent* New(int, const vector<string> &, Configuration const *, WorkloadComponent *) = 0;
      };
      template<class WC> class Factory : FactoryBase {
         public:
            Factory(const string &kind) : FactoryBase(kind) {}
            virtual WorkloadComponent* New(int nodes, const vector<string> &options, Configuration const * config, WorkloadComponent *upstrm) {
               return new WC(nodes, options, config, upstrm);
            }
      };
};

