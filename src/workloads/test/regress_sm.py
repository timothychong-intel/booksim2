#!/usr/intel/bin/python
#
# regression tests for Booksim small message coalescing (SMC) workloads
#
from simbench import *

# map from coalescing degree to number of flits, see also msg.cpp
c2f = { 4:12, 12:25 }

exps = [
    Booksim('sm_fat', name='sm_fat_c%d'%c,
        sm_rate_ghz=1.0, k=8, deadlock_warn_timeout=10000, vc_buf_size=1000,
        coalescing_degree=c, packet_size=c2f[c], outport_util_estimator=0.001)
    for c in [4, 12]
]
single_c_bms = [
    # single traffic class SM only
    BookSynth(name='001', injection_process='sm',     injection_rate=0.05),
    BookSynth(name='002', injection_process='sm',     injection_rate=0.35),
    BookSynth(name='003', injection_process='sm_end', injection_rate=0.05),
    BookSynth(name='004', injection_process='sm_end', injection_rate=0.35),

    # single traffic class SM only, opportunistic with rate threshold
    BookSynth(name='005', outport_util_threshold=0.2, injection_process='sm',     injection_rate=0.05),
    BookSynth(name='006', outport_util_threshold=0.2, injection_process='sm',     injection_rate=0.35),
    BookSynth(name='007', outport_util_threshold=0.2, injection_process='sm_end', injection_rate=0.05),
    BookSynth(name='008', outport_util_threshold=0.2, injection_process='sm_end', injection_rate=0.35),

    # single traffic class SM only, opportunistic with age threshold

    # single traffic class SM only, RCDC
    BookSynth(name='009', injection_process='sm', rcu_type='rc_dc', internal_speedup=1.0, injection_rate=0.05),
    BookSynth(name='010', injection_process='sm', rcu_type='rc_dc', internal_speedup=1.0, injection_rate=0.45),
    BookSynth(name='011', injection_process='sm', rcu_type='rc_dc', internal_speedup=3.5, injection_rate=0.45),
    BookSynth(name='012', injection_process='sm', rcu_type='rc_dc', internal_speedup=5.0, injection_rate=0.45),

    # single traffic class SM only, component, ethernet
    ##BookSynth(name='017', injection_process="'component(random(bernoulli),sm(0),packetize(66,18,1500,12.5))'",packet_size=20,injection_rate=0.05),
    # single traffic class SM only, component, XeLink
    ##BookSynth(name='021', injection_process="'component(random(bernoulli),sm(0),packetize(8,0,9999999,8.0))'",packet_size=20,injection_rate=0.05),
]
multi_c_bms = [
    # multi traffic class SM + Bernoulli background. Double quotes to protect {} from the shell!
    BookSynth(name='013', classes=2, packet_size="'{12,88}'", injection_process="'{sm,bernoulli}'", injection_rate="'{0.05,0.005}'"),
    BookSynth(name='014', classes=2, packet_size="'{12,88}'", injection_process="'{sm,bernoulli}'", injection_rate="'{0.35,0.005}'"),
    BookSynth(name='015', classes=2, packet_size="'{12,88}'", injection_process="'{sm,bernoulli}'", injection_rate="'{0.05,0.020}'"),
    BookSynth(name='016', classes=2, packet_size="'{12,88}'", injection_process="'{sm,bernoulli}'", injection_rate="'{0.35,0.020}'"),

    # multi traffic class SM + Bernoullie background, RCDC
    BookSynth(name='017', classes=2, packet_size="'{12,88}'", injection_process="'{sm,bernoulli}'", injection_rate="'{0.05,0.005}'", rcu_type='rc_dc', internal_speedup=3.0),
    BookSynth(name='018', classes=2, packet_size="'{88,12}'", injection_process="'{bernoulli,sm}'", injection_rate="'{0.005,0.05}'", rcu_type='rc_dc', internal_speedup=3.0),
    BookSynth(name='019', classes=2, packet_size="'{12,88}'", injection_process="'{sm,bernoulli}'", injection_rate="'{0.35,0.020}'", rcu_type='rc_dc', internal_speedup=5.0),
    BookSynth(name='020', classes=2, packet_size="'{88,12}'", injection_process="'{bernoulli,sm}'", injection_rate="'{0.020,0.35}'", rcu_type='rc_dc', internal_speedup=5.0),
]

RunSimJobs(sys.argv) (
    experiments=[exps[0]], benchmarks=single_c_bms+multi_c_bms,
    stats=['SM BW','Flit BW','Packet BW'], statype='f', rclass=BooksimResults, myprint='xbs'
)
RunSimJobs(sys.argv) (
    experiments=[exps[1]], benchmarks=single_c_bms, # no multi-class for the c=12 case
    stats=['SM BW','Flit BW','Packet BW'], statype='f', rclass=BooksimResults, myprint='xbs'
)

## sm_dev 8d39c5b5b986e706f17107336c42815584339585 results:
#
# sm_fat_c4:
# 001,0.049237,0.147727,0.012309
# 002,0.266373,0.799099,0.066593
# 003,0.049958,0.149864,0.012490
# 004,0.278394,0.835196,0.069598
# 005,0.050200,0.239291,0.028442
# 006,0.053663,0.207067,0.021819
# 007,0.050307,0.210241,0.022901
# 008,0.278415,0.835198,0.069604
# 009,0.050316,0.150949,0.012579
# 010,0.102900,0.308700,0.025725
# 011,0.204281,0.612877,0.051070
# 012,0.195525,0.586608,0.048881
# 013,0.049475,0.443618,0.005040
# 014,0.074494,0.443655,0.005040
# 015,0.043905,0.587974,0.006681
# 016,0.046927,0.586613,0.006668
# 017,0.049591,0.450700,0.005117
# 018,0.049794,0.149426,0.012448
# 019,0.112208,0.587636,0.006677
# 020,0.112983,0.338909,0.028246
# sm_fat_c12:
# 001,0.049636,0.103382,0.004136
# 002,0.349675,0.728546,0.029140
# 003,0.050606,0.105432,0.004217
# 004,0.349481,0.728046,0.029123
# 005,0.050036,0.244999,0.029927
# 006,0.313608,0.653584,0.026134
# 007,0.050364,0.209110,0.022585
# 008,0.349575,0.728271,0.029131
# 009,0.050333,0.104859,0.004194
# 010,0.108743,0.226547,0.009062
# 011,0.206892,0.431313,0.017241
# 012,0.193753,0.403907,0.016146
