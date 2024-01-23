// $Id$

/*
 Copyright (c) 2007-2015, Trustees of The Leland Stanford Junior University
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.
 Redistributions in binary form must reproduce the above copyright notice, this
 list of conditions and the following disclaimer in the documentation and/or
 other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef _LOSSY_OQ_ROUTER_HPP_
#define _LOSSY_OQ_ROUTER_HPP_

#include <string>
#include <deque>
#include <queue>
#include <list>
#include <set>
#include <map>

#include "router.hpp"
#include "routefunc.hpp"

using namespace std;

class VC;
class Flit;
class Credit;
class Buffer;
class BufferState;
class SwitchMonitor;
class BufferMonitor;

class LossyOQRouter : public Router {

  int _vcs;

  bool _active;

  int _routing_delay;
  int _crossbar_latency;

  map<int, Flit *> _in_queue_flits;

  deque<pair<int, pair<Credit *, int> > > _proc_credits;

  deque<pair<int, pair<Flit *, pair<int, int> > > > _crossbar_flits;

  map<int, Credit *> _out_queue_credits;

  vector<Buffer *> _buf;
  vector<BufferState *> _next_buf;

  // vector to hold the desired output port of the last head flit to enter the
  // given input port.  This is used to slot body and tail flits correctly and
  // assumes that they are always contiguous (not interleaved with flits from
  // other packets).
  vector<int> _last_head_flit_output_port;

  tRoutingFunction   _rf;

  int _output_buffer_size;
//  vector<queue<Flit *> > _output_buffer;
  vector<list<Flit *> > _output_buffer;
  vector<int> _current_pid_output_in_progress;
  vector<queue<Credit *> > _credit_buffer;

  int _total_buffer_size;
  int _total_buffer_occupancy;
  vector<int> _output_buffer_occupancy;

  bool _use_endpoint_crediting;
  vector< vector< list< Flit * >::iterator > > _oq_insertion_iters;
  vector<int> _input_insertion_pointing_at_output_buffer_head;

  vector<bool> _drop_packet_at_input;
  float _random_packet_drop_rate;

  bool _ReceiveFlits( );
  bool _ReceiveCredits( );

  virtual void _InternalStep( );

  void _InputQueuing( );

  void _SwitchEvaluate( );
  void _SwitchUpdate( );

  void _OutputQueuing( );

  void _SendFlits( );
  void _SendCredits( );

  // ----------------------------------------
  //
  //   Router Power Modellingyes
  //
  // ----------------------------------------

  SwitchMonitor * _switchMonitor ;
  BufferMonitor * _bufferMonitor ;

public:

  LossyOQRouter( Configuration const & config,
	    Module *parent, string const & name, int id,
	    int inputs, int outputs );

  virtual ~LossyOQRouter( );

  virtual void AddOutputChannel(FlitChannel * channel, CreditChannel * backchannel);

  virtual void ReadInputs( );
  virtual void WriteOutputs( );

  void Display( ostream & os = cout ) const;

  virtual int GetUsedCredit(int o) const;
  virtual int GetBufferOccupancy(int i) const;

#ifdef TRACK_BUFFERS
  virtual int GetUsedCreditForClass(int output, int cl) const;
  virtual int GetBufferOccupancyForClass(int input, int cl) const;
#endif

  virtual vector<int> UsedCredits() const;
  virtual vector<int> FreeCredits() const;
  virtual vector<int> MaxCredits() const;

  SwitchMonitor const * GetSwitchMonitor() const {return _switchMonitor;}
  BufferMonitor const * GetBufferMonitor() const {return _bufferMonitor;}

};

#endif
