/*
 * workload component for a generic random all-to-all traffic
 *
 *  * ported for Booksim scalable workload model
 *
 *
 *      Author: Timothy Chong
 */

#include "swm_histo.hpp"

double SwmHisto::gen_interval(){
    double gen = (*expo)(generator);
    return gen;
}

void SwmHisto::send_packets(int num_of_packets, unsigned int run_till){
    while(num_of_packets && !gShouldSkipDrain) {
        if (getTime() > this->last_injection_time + this->send_interval){
            this->send_interval = gen_interval();
            this->last_injection_time = getTime();
            put(this->packet_size, (*intdist)(generator));
            num_of_packets --;
        } else {
            set_time(ceil(this->last_injection_time + this->send_interval));
        }

        if (lgp.getTime() >= run_till){
            cout << _me <<": drain now" << lgp.getTime() << endl << flush;
            gSimEnabled = false;
            gShouldSkipDrain = true;
        }
    }
}
void SwmHisto::behavior(int argc, char *argv[]){
    lgp.lgp_init(this, _me, _np);
    spmat.init_spmat(&lgp, _me, _np);
    std.init_std_options(&lgp, &spmat, _me, _np);


    this->packet_size = getConfig()->GetInt("packet_size");
    //this->work_time = getConfig()->GetInt("swm_work_time");

    cout << "write packet size" << this->packet_size << endl << flush;

    int header_size = 2;
    // In packet_per cycle
    double per_endpoint_bandwdith = 1.0 / (double)(packet_size + header_size);
    //double variance = 1.0 / (double)(packet_size + header_size) / _np * 0.1;

    generator.seed(123 + _me);
    expo = new std::exponential_distribution<double>(per_endpoint_bandwdith);
    intdist = new std::uniform_int_distribution<int>(0, _np - 1);

    this->last_injection_time = 0.0;
    put(this->packet_size, (*intdist)(generator));
    this->send_interval = gen_interval();

    unsigned int start_time = 900000;
    unsigned int run_for = 100000;

    gLastClearStatTime = start_time;
    send_packets(1000000, start_time + run_for);

    cout <<  _me << ": Ending Histo application at " << lgp.getTime() << endl;
}
