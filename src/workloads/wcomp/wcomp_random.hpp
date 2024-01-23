/*
 * Random number workload traffic generator, using Booksim's built-in injection process and traffic patterns
 *
 * C. Beckmann (c) Intel, 2023
 */
#include "wkld_comp.hpp"
#include "injection.hpp"
#include "traffic.hpp"

class RandomWorkloadGenerator : public WorkloadComponent,
                                private GeneratorWorkloadMessage::Factory
{
    static WorkloadComponent::Factory<RandomWorkloadGenerator> _factory;

    double const _wr_fraction;               // copy of key Booksim knobs
    bool   const _use_rdwr;

    string             _injection_process;
    InjectionProcess * _inject;              // Booksim injection process
    TrafficPattern *   _pattern;             // Booksim traffic pattern

    // knob parsing helper functions
    #define GET_BY_CLASS(TYPE, NAME, GETARRAY, GETITEM) \
    TYPE NAME(  Configuration const *cfg, const char *n, int tc) { \
        auto a = cfg->GETARRAY(n); \
        if (a.empty()) return cfg->GETITEM(n); \
        a.resize(tc+1, a.back()); \
        return a[tc]; \
    }
    GET_BY_CLASS(string, _get_str_by_class,   GetStrArray,   GetStr);
    GET_BY_CLASS(int,    _get_int_by_class,   GetIntArray,   GetInt);
    GET_BY_CLASS(double, _get_float_by_class, GetFloatArray, GetFloat);
    #undef GET_BY_CLASS

  public:
    // Options are:
    //   injection process (same options as booksim knob injection_process)
    // Also uses booksim knobs:
    //   traffic, injection_rate, injection_rate_uses_flits (must be 0!), use_read_write, write_fraction,
    //   packet_size, read_request_size, read_reply_size, write_request_size, write_reply_size
    // Assumes these are all scalars (for now), i.e. there is only one traffic class.
    // Also, there is currently no support for packet_size_rate.
    RandomWorkloadGenerator(int nodes, const vector<string> &options, Configuration const * config, WorkloadComponent *upstrm);
    void Init(int pes, Configuration const *config);
    bool test(int src) { return _inject->test(src); }
  private:
    WorkloadMessagePtr _get_new(int src);
};
