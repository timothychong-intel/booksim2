#!/usr/intel/bin/python
#
# regression tests for Booksim component workloads
#
from simbench import *

run = RunSimJobs(sys.argv)

sim_swm_fat = Booksim('swm_fat', k=8, warmup_periods=0, max_samples=1, deadlock_warn_timeout=10000)

bms = [
    # these four should give the same results:
    BookSynth(name='001', injection_process='bernoulli'),
    BookSynth(name='002', injection_process="'component(random(bernoulli))'", swm_app_run_mode=0),
    BookSynth(name='003', injection_process="'component(random(bernoulli),packetize(0,1,999999,1.0))'", swm_app_run_mode=0),
    BookSynth(name='004', injection_process="'component(random(bernoulli),trace(get,message))'", swm_app_run_mode=0),

    # barrier sync
    BookSynth(name='005', injection_process="'component(SWM(barrier))'", swm_args="'{100,10}'", roi=1, roi_begin=1111, roi_end=2222),

    # 6 & 7 and 8 & 9 pairs should give the same result
    BookSynth(name='006', injection_process="'component(SWM(randperm))'",
              swm_app_run_mode=1, use_read_write=1, swm_args="'{-n100}'", roi=1, roi_begin=1111, roi_end=2222),
    BookSynth(name='007', injection_process="'component(SWM(randperm),trace(get,eject))'",
              swm_app_run_mode=1, use_read_write=1, swm_args="'{-n100}'", roi=1, roi_begin=1111, roi_end=2222),
    BookSynth(name='008', injection_process="'component(SWM(randperm),packetize(78,64,1500,12.5))'",
              swm_app_run_mode=1, use_read_write=1, swm_args="'{-n100}'", roi=1, roi_begin=1111, roi_end=2222),
    BookSynth(name='009', injection_process="'component(SWM(randperm),Mppn(1),packetize(78,64,1500,12.5))'",
              swm_app_run_mode=1, use_read_write=1, swm_args="'{-n100}'", roi=1, roi_begin=1111, roi_end=2222),
    BookSynth(name='010', injection_process="'component(SWM(randperm),Mppn(3),packetize(78,64,1500,12.5))'",
              swm_app_run_mode=1, use_read_write=1, swm_args="'{-n100}'", roi=1, roi_begin=1111, roi_end=2222),

    # multi-class. All 3 in each of these triples should give the same result
    BookSynth(name='011', swm_app_run_mode=0, classes=2, injection_process=  "bernoulli",                                injection_rate="'{0.1,0.2}'", use_read_write="'{0,1}'", write_fraction=0.3),
    BookSynth(name='012', swm_app_run_mode=0, classes=2, injection_process= "'component(random(bernoulli))'",            injection_rate="'{0.1,0.2}'", use_read_write="'{0,1}'", write_fraction=0.3),
    BookSynth(name='013', swm_app_run_mode=0, classes=2, injection_process= "'{component(random(bernoulli))}'",          injection_rate="'{0.1,0.2}'", use_read_write="'{0,1}'", write_fraction=0.3),
    #
    BookSynth(name='014', swm_app_run_mode=0, classes=2, injection_process="'{bernoulli,bernoulli}'",                    injection_rate="'{0.3,0.5}'", use_read_write="'{0,1}'", write_fraction=0.3),
    BookSynth(name='015', swm_app_run_mode=0, classes=2, injection_process="'{component(random(bernoulli)),bernoulli}'", injection_rate="'{0.3,0.5}'", use_read_write="'{0,1}'", write_fraction=0.3),
    BookSynth(name='016', swm_app_run_mode=0, classes=2, injection_process="'{bernoulli,component(random(bernoulli))}'", injection_rate="'{0.3,0.5}'", use_read_write="'{0,1}'", write_fraction=0.3),
    #
    BookSynth(name='017', swm_app_run_mode=0, classes=2, injection_process="'{bernoulli,bernoulli}'",                    injection_rate="'{0.1,0.2}'", use_read_write="'{1,1}'", write_fraction="'{0.3,0.7}'"),
    BookSynth(name='018', swm_app_run_mode=0, classes=2, injection_process="'{component(random(bernoulli)),bernoulli}'", injection_rate="'{0.1,0.2}'", use_read_write="'{1,1}'", write_fraction="'{0.3,0.7}'"),
    BookSynth(name='019', swm_app_run_mode=0, classes=2, injection_process="'{bernoulli,component(random(bernoulli))}'", injection_rate="'{0.1,0.2}'", use_read_write="'{1,1}'", write_fraction="'{0.3,0.7}'"),
    #
    BookSynth(name='020', swm_app_run_mode=0, classes=2, injection_process="'{bernoulli,bernoulli}'",                    injection_rate="'{0.2,0.3}'", use_read_write="'{1,1}'", write_fraction="'{0.5,0.1}'"),
    BookSynth(name='021', swm_app_run_mode=0, classes=2, injection_process="'{component(random(bernoulli)),bernoulli}'", injection_rate="'{0.2,0.3}'", use_read_write="'{1,1}'", write_fraction="'{0.5,0.1}'"),
    BookSynth(name='022', swm_app_run_mode=0, classes=2, injection_process="'{bernoulli,component(random(bernoulli))}'", injection_rate="'{0.2,0.3}'", use_read_write="'{1,1}'", write_fraction="'{0.5,0.1}'"),

    # the "random" component should be able to instantiate multiple instances per node
    BookSynth(name='023', injection_process="'component(random(bernoulli),Mppn(8))'", swm_app_run_mode=0),

    # adding some local latency in both directions
    BookSynth(name='024', injection_process="'component(SWM(barrier),latency(20,0))'",  swm_args="'{100,10}'", roi=1, roi_begin=1111, roi_end=2222),
    BookSynth(name='025', injection_process="'component(SWM(barrier),latency(0,20))'",  swm_args="'{100,10}'", roi=1, roi_begin=1111, roi_end=2222),
    BookSynth(name='026', injection_process="'component(SWM(barrier),latency(10,10))'", swm_args="'{100,10}'", roi=1, roi_begin=1111, roi_end=2222),
]

run(experiments=[sim_swm_fat], benchmarks=bms, stats=['Flit BW'], statype='f', rclass=BooksimResults)
