/*
 * workload component implementation
 *
 * H. Dogan (c) Intel, 2023
 *
 */
#include "wkld_comp.hpp"
#include "swm_test1.hpp"
#include "swm_barrier.hpp"
#include "swm_ig.hpp"
#include "swm_histo.hpp"
#include "swm_randperm.hpp"
#include "swm_permute_matrix.hpp"
#include "swm_transpose_matrix.hpp"
#include "swm_toposort.hpp"
#include "swm_basic.hpp"

// scalable workload model - primary traffic generator
class ScalableWorkloadModel : public WorkloadComponent, public RoI,
                              private GeneratorWorkloadMessage::Factory
{
   string _name;        // name of the SWM
   //static WorkloadComponent::Factory<ScalableWorkloadModel, "SWM"> _factory;
   static WorkloadComponent::Factory<ScalableWorkloadModel> _factory;

   // similar to SwmInjectionProcess
   vector<SwmThread *> _thread;        // array of SWM thread objects
   // static data
   static map<string, SwmThread::factory_base*> _wfact; // workload factory classes


   public:
   ScalableWorkloadModel(int nodes, const vector<string> &options, Configuration const * const config, WorkloadComponent *upstrm);

   void FunctionalSim();
   void Init(int pes, Configuration const * const config);

   bool test(int src)
   {
      return _thread[src]->has_packet(GetSimTime());
   }
   WorkloadMessagePtr _get_new(int src) {
      auto m =  _thread[src]->get_packet();
      assert(m.source == src);
      int marker = m.marker;
      SwmRoI(marker, src);
      if(IsRoI(src, marker)) {
        _thread[src]->set_done();
      }
      return new GeneratorWorkloadMessage(this, m.source, m.dest, m.type, false, m.size);
   }
   void next(int src)
   {
      WorkloadComponent::next(src);
      _thread[src]->next(GetSimTime());
   }
   void eject(WorkloadMessagePtr m)
   {
      bool is_reply = m->IsReply();
      bool is_send  = m->Type() == WorkloadMessage::SendRequest && !is_reply;
      int source = m->Source();
      int dest   = m->Dest();
      int size   = m->Size();
      if (is_reply) {
         _thread[dest]->reply(
            GetSimTime(),
            SwmThread::pkt_s(
               {m->Type(), 0, source, dest, size, true}));
      }
      else if (is_send) { // handle incoming SEND message
         _thread[dest]->sendin(
            GetSimTime(),
            SwmThread::pkt_s(
               {m->Type(), 0, source, dest, size, false}));
      }
   }
};


// map from names to workload-specific factory objects
map<string, SwmThread::factory_base*> ScalableWorkloadModel::_wfact = {
   {"test1",    new SwmThread::factory<SwmTest1>},
   {"ig",       new SwmThread::factory<SwmIg>},
   {"histo",    new SwmThread::factory<SwmHisto>},
   {"barrier",  new SwmThread::factory<SwmBarrier>},
   {"randperm", new SwmThread::factory<SwmRandPerm>},
   {"permmatrix", new SwmThread::factory<SwmPermMatrix>},
   {"transmatrix", new SwmThread::factory<SwmTransposeMatrix>},
   {"toposort", new SwmThread::factory<SwmToposort>},
   {"basic",  new SwmThread::factory<SwmBasic>},
};


// instantiate ScalableWorkloadModel factory object
WorkloadComponent::Factory<ScalableWorkloadModel> ScalableWorkloadModel::_factory("SWM");

/*
 * Scalable Workload Model (SWM) -- traffic generator
 */
ScalableWorkloadModel::ScalableWorkloadModel(int nodes, const vector<string> &options, Configuration const * const config, WorkloadComponent *upstrm)
   : RoI(config), GeneratorWorkloadMessage::Factory(config, gInjectorTrafficClass), _name(options[0]), _thread(0)
{}


void ScalableWorkloadModel::Init(int pes, Configuration const * const config)
{
   _thread.resize(pes);
   try {
      auto factory = _wfact.at(_name);
      for (int i=0; i<pes; ++i)
         _thread[i] = factory->make(i, pes, config);
   }
   catch (const out_of_range &) {
      cerr << "Unkown SWM workload type: " << _name << endl;
      exit(-1);
   }
   WorkloadComponent::Init(pes, config);
   RoI::Init(pes);
   if(config->GetInt("roi")) {
      FunctionalSim();
   }
}


void ScalableWorkloadModel::FunctionalSim() {

   typedef uint64_t                    cycle_t;
   typedef SwmThread::pkt_s            packet_t;
   typedef multimap<cycle_t, packet_t> packet_list_t;
   typedef pair<cycle_t, packet_t>     plist_item_t;

   bool                    done = false;       // is simulation done?
   cycle_t                 now = 0;        // current simulation time
   packet_list_t           inflight;   // in-flight packets
   std::vector<bool> t_done(_thread.size());
   for(auto i:t_done)
      i = false;

   while(!gSimEnabled && !done) {

      done = true;

      // handle incoming packets from the fabric
      packet_list_t::iterator pi;
      while ((pi = inflight.find(now)) != inflight.end()) {
         done = false;
         packet_t & pkt = pi->second;
         if (pkt.is_reply) {
            // if a reply, provide reply to waiting process
            _thread[pkt.dest]->reply(now, pkt);
         }
         else if (pkt.type == WorkloadMessage::SendRequest) {
            // provide incoming SEND to destination process
            _thread[pkt.dest]->sendin(now, pkt);
            // create an ack reply packet back to initiator
            inflight.insert(plist_item_t(now + 1, pkt.reply()));
         }
         else {
            // if a GET request, create a reply packet back to initiator
            // if a PUT request, create an ack reply packet back to initiator
            inflight.insert(plist_item_t(now + 1, pkt.reply()));
         }
         inflight.erase(pi);
      }

      // get outgoing packets to the fabric
      for (auto t : _thread) {
         int tid = t->get_id();
         if (t->has_packet(now) && !t_done[tid]) {
            done = false;

            auto p = t->get_packet();
            int marker = p.marker;
            SwmRoI(marker, tid);
            if(IsRoI(tid, marker)) {
               t_done[tid] = true;
               t->set_done();
            }

            bool is_dummy = p.type == WorkloadMessage::DummyRequest;
            if(!is_dummy && !t->is_quiet()) {
               inflight.insert(plist_item_t(now + 1, p));
            }

            if(!t->is_done())
               t->next(now);
         }
         else if (!t->is_done())
            done = false;
      }

      now++;
   }

   // Drain the inflight messages
   while(!inflight.empty()) {

      // handle incoming packets from the fabric
      packet_list_t::iterator pi;
      while ((pi = inflight.find(now)) != inflight.end()) {
         packet_t & pkt = pi->second;
         if (pkt.is_reply) {
            // if a reply, provide reply to waiting process
            _thread[pkt.dest]->reply(now, pkt);
         }
         else if (pkt.type == WorkloadMessage::SendRequest) {
            // provide incoming SEND to destination process
            _thread[pkt.dest]->sendin(now, pkt);
            // create an ack reply packet back to initiator
            inflight.insert(plist_item_t(now + 1, pkt.reply()));
         }
         else {
            // if a GET request, create a reply packet back to initiator
            // if a PUT request, create an ack reply packet back to initiator
            inflight.insert(plist_item_t(now + 1, pkt.reply()));
         }
         inflight.erase(pi);
      }
      now++;
   }

   //
   for (auto t : _thread) {
      t->SwmReset();
   }

   printf("Functional Simulation Completed\n");
}

