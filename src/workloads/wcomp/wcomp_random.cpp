/*
 * Random number workload traffic generator, using Booksim's built-in injection process and traffic patterns
 *
 * C. Beckmann (c) Intel, 2023
 */
#include "wcomp_random.hpp"
#include "random_utils.hpp"

WorkloadComponent::Factory<RandomWorkloadGenerator> RandomWorkloadGenerator::_factory("random");

RandomWorkloadGenerator::RandomWorkloadGenerator(int nodes, const vector<string> &options, Configuration const * config, WorkloadComponent *upstrm)
  : GeneratorWorkloadMessage::Factory(config, gInjectorTrafficClass),
    _wr_fraction(_get_float_by_class(config, "write_fraction", traffic_class)),
    _use_rdwr(_get_int_by_class(config, "use_read_write", traffic_class)),
    _injection_process(options[0]), _inject(0),  _pattern(0)
{
    // injection rate is always in terms of packets
    assert(0 == config->GetInt("injection_rate_uses_flits"));
    // no support for packet_size_rate currently
    assert(config->GetStr("packet_size_rate").empty() && 1 == config->GetInt("packet_size_rate"));
}

void RandomWorkloadGenerator::Init(int pes, Configuration const *config)
{
    _inject = InjectionProcess::New(_injection_process, pes, config->GetFloatArray("injection_rate"), config);
    _pattern = TrafficPattern::New(_get_str_by_class(config, "traffic", traffic_class), pes, config);
}

WorkloadMessagePtr RandomWorkloadGenerator::_get_new(int src)
{
    auto dest = _pattern->dest(src);
    WorkloadMessage::msg_t type = WorkloadMessage::AnyRequest;
    if (_use_rdwr) {
        bool is_write = (RandomFloat() < _wr_fraction);
        type = is_write ? WorkloadMessage::PutRequest : WorkloadMessage::GetRequest;
    }
    return new GeneratorWorkloadMessage(this, src, dest, type); // size inserted based on Booksim knobs
}
