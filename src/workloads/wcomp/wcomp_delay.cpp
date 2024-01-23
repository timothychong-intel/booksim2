/*
 * workload component for adding on-node latency
 *
 * C. Beckmann (c) Intel, 2023
 * 
 */
#include "globals.hpp"
#include "wkld_comp.hpp"
#include <vector>
#include <deque>
#include <string>

//////////////////
// On-node latency - traffic modifier
class OnNodeLatency : public WorkloadComponent
{
    static WorkloadComponent::Factory<OnNodeLatency> _factory;

    WorkloadComponent *const _upstream;
    const int _out_latency;
    const int _in_latency;

    // delay lines containing <message,time> pairs, indexed by node ID
    typedef pair<WorkloadMessagePtr, int> qitem_t;
    vector<deque<qitem_t> > _outgoing; 
    vector<deque<qitem_t> > _incoming;

  public:
    // two configuration parameters: outgoing latency, incoming latency
    OnNodeLatency(int nodes, const vector<string> &options, Configuration const * const config, WorkloadComponent * upstrm)
      : _upstream(upstrm),
        _out_latency(stoi(options[0])),
        _in_latency(stoi(options[1])),
        _outgoing(nodes),
        _incoming(nodes)
    {}
    void Init(int nodes, Configuration const *config) {
        _upstream->Init(nodes, config);
    }
    bool test(int src) {
        // check upstream and insert into outgoing delay line
        if (_upstream->test(src)) {
            _outgoing[src].push_front(qitem_t(_upstream->get(src), GetSimTime() + _out_latency));
            _upstream->next(src);
        }
        // check if the head of the delay line for this source is ready this cycle
        if (_outgoing[src].empty()) return false;
        auto& head(_outgoing[src].back());
        return GetSimTime() >= head.second;
    }
    void next(int src) {
        WorkloadComponent::next(src); // required
        // pop the head of the delay line for this source
        _outgoing[src].pop_back();
    }
    void eject(WorkloadMessagePtr m) {
        // insert into the return delay line for the destination
        _incoming[m->Dest()].push_front(qitem_t(m, GetSimTime() + _in_latency));
        // eject the heads of the delay lines for all destinations
        for (auto& q : _incoming) {
            if (q.empty()) continue;
            auto& head(q.back());
            if (GetSimTime() < head.second) continue;
            _upstream->eject(head.first);
            q.pop_back();
        }
    }
  private:
    virtual WorkloadMessagePtr _get_new(int src) {
        // return the head of the delay line for this source
        return _outgoing[src].back().first;
    }
};

WorkloadComponent::Factory<OnNodeLatency> OnNodeLatency::_factory("latency");
