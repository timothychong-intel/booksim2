/*
 * workload component for Small Message Coalescing  
 *
 * C. Beckman (c) Intel, 2023
 * 
 */

#include "wkld_comp.hpp"

//////////////////
// small message coalescing - traffic modifier
class CoalescingEndpoint : public WorkloadComponent
{
  public:
   class Coalesced : public ModifierWorkloadMessage<Coalesced>
   {
      std::list<WorkloadMessagePtr> _sm_list;
     public:
      // TODO: constructors and Add() should set _contents to first item in _sm_list, for API proxy calls
      Coalesced();
      Coalesced(const Coalesced &);
      Coalesced(WorkloadMessagePtr);
      bool IsFull() const;
      void Add(WorkloadMessagePtr);
      void Clear();

      typedef std::list<WorkloadMessagePtr>::iterator iterator;
      iterator begin() { return _sm_list.begin(); }
      iterator end() { return _sm_list.end();   }
   };

   private:
      WorkloadComponent * _upstream; // upstream traffic generator
      vector<Coalesced> _cbufs;      // coalescing buffer - one per node

   public:
      // see if there is anything to send
      bool test(int src)
      {
         // if upstream has anything to send, get it and add to coalescing buffer
         if (_upstream->test(src))
            _cbufs[src].Add(_upstream->get(src));
         // return SUCCESS if the coalescing buffer is full
         return _cbufs[src].IsFull();
      }
      // get outgoing packet info
      WorkloadMessagePtr _get_new(int src)
      {
         return new Coalesced(_cbufs[src]); // make a copy
      }
      // done this cycle
      void next(int src)
      {
         WorkloadComponent::next(src);
         _upstream->next(src);
         _cbufs[src].Clear();
      }
      // receive incoming traffic from fabric
      void eject(WorkloadMessagePtr m) {
         auto cm = dynamic_cast<Coalesced*>(m.get());
         assert(cm);
         for (auto sm : *cm) // de-coalesce to upstream generator:
            _upstream->eject(sm);
         delete cm;
      }
};

