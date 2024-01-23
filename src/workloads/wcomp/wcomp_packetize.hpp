/*
 * workload component for adding packetization overhead
 *
 * H. Dogan (c) Intel, 2023
 * 
 */

#include "wkld_comp.hpp"

//////////////////
// Packetization Layer - traffic modifier
class Packetize : public WorkloadComponent
{
    private:
    static WorkloadComponent::Factory<Packetize> _factory;
 
    static float _flit_size;
    static int _fabric_overhead;
    static int _max_payload;
    static int _min_payload;
    string _fabric;
 
    WorkloadComponent * _upstream;
 
    static int _flits(int bytes);
 
    // encapsulated message with flit info 
    class Message : public ModifierWorkloadMessage<Message>
    {
       friend class Packetize;
      public:
       Message(WorkloadMessagePtr msg) : ModifierWorkloadMessage<Message>(msg) {}
       int Size() const { return _flits(_contents->Size()); }
    };
 
   public:
    Packetize(int nodes, const vector<string> &options, Configuration const * const config, WorkloadComponent * upstrm);
    void Init(int nodes, Configuration const * const config) { _upstream->Init(nodes, config); }
    bool test(int src) { return _upstream->test(src); }
    WorkloadMessagePtr _get_new(int src) { return new Message(_upstream->get(src)); }
    void next(int src);
    void eject(WorkloadMessagePtr m);
};
