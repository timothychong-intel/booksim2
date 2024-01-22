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

#include "lossy_oq_router.hpp"

#include <string>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <cassert>
#include <limits>

#include "globals.hpp"
#include "random_utils.hpp"
#include "vc.hpp"
#include "routefunc.hpp"
#include "outputset.hpp"
#include "buffer.hpp"
#include "buffer_state.hpp"
#include "switch_monitor.hpp"
#include "buffer_monitor.hpp"

LossyOQRouter::LossyOQRouter( Configuration const & config, Module *parent,
		    string const & name, int id, int inputs, int outputs )
: Router( config, parent, name, id, inputs, outputs ), _active(false)
{
  _vcs         = config.GetInt( "num_vcs" );

  _routing_delay    = config.GetInt( "routing_delay" );

  // Pipelined latency, every flit gets delayed by this amount but can stream
  // back-to-back.
  _crossbar_latency = config.GetInt("crossbar_latency");

  // Routing
  string const rf = config.GetStr("routing_function") + "_" + config.GetStr("topology");
  map<string, tRoutingFunction>::const_iterator rf_iter = gRoutingFunctionMap.find(rf);
  if(rf_iter == gRoutingFunctionMap.end()) {
    Error("Invalid routing function: " + rf);
  }
  _rf = rf_iter->second;

  // Alloc VC's
  _buf.resize(_inputs);
  for ( int i = 0; i < _inputs; ++i ) {
    ostringstream module_name;
    module_name << "buf_" << i;
    _buf[i] = new Buffer(config, _outputs, this, module_name.str( ) );
    module_name.str("");
  }

  // Alloc next VCs' buffer state
  _next_buf.resize(_outputs);
  for (int j = 0; j < _outputs; ++j) {
    ostringstream module_name;
    module_name << "next_vc_o" << j;
    _next_buf[j] = new BufferState( config, this, module_name.str( ) );
    module_name.str("");
  }


  _use_endpoint_crediting = config.GetInt("use_endpoint_crediting");


  // Output queues
  _output_buffer_size = int(config.GetInt("output_buffer_size_in_kb") * 1000 / gFlitSize);
  _output_buffer.resize(_outputs);
  _output_buffer_occupancy.resize(_outputs, 0);
  _credit_buffer.resize(_inputs);

  _total_buffer_size = config.GetInt("router_total_buffer_size");
  _total_buffer_occupancy = 0;

  _last_head_flit_output_port.resize(_inputs);
  _oq_insertion_iters.resize(_outputs);
  for (int output = 0; output < _outputs; ++output) {
    _oq_insertion_iters[output].resize(_inputs);
    for (int input = 0; input < _inputs; ++input) {
      _oq_insertion_iters[output][input] = _output_buffer[output].end();
    }
  }
  _input_insertion_pointing_at_output_buffer_head.resize(_outputs, -1);
  _current_pid_output_in_progress.resize(_outputs, -1);

  _bufferMonitor = new BufferMonitor(inputs, _classes);
  _switchMonitor = new SwitchMonitor(inputs, outputs, _classes);


  _random_packet_drop_rate = config.GetFloat( "switch_drop_rate" );

  // Initialize the state to drop incoming flits of a multi-flit packet.  The
  // decision to drop is made when the head flit is received.  This state is
  // set at that point and used by subsequent flits to indicate they must be
  // dropped.  When a tail flit is received, this state is reset to false.
  _drop_packet_at_input.resize(_inputs);
  for (int i = 0; i < _inputs; ++i) {
    _drop_packet_at_input[i] = false;
  }

}

LossyOQRouter::~LossyOQRouter( )
{

  if(gPrintActivity) {
    cout << Name() << ".bufferMonitor:" << endl ;
    cout << *_bufferMonitor << endl ;

    cout << Name() << ".switchMonitor:" << endl ;
    cout << "Inputs=" << _inputs ;
    cout << "Outputs=" << _outputs ;
    cout << *_switchMonitor << endl ;
  }

  for(int i = 0; i < _inputs; ++i)
    delete _buf[i];

  for(int j = 0; j < _outputs; ++j)
    delete _next_buf[j];

  delete _bufferMonitor;
  delete _switchMonitor;
}

void LossyOQRouter::AddOutputChannel(FlitChannel * channel, CreditChannel * backchannel)
{
  int min_latency = 1 + _crossbar_delay + channel->GetLatency() + _routing_delay + backchannel->GetLatency()  + _credit_delay;
  _next_buf[_output_channels.size()]->SetMinLatency(min_latency);
  Router::AddOutputChannel(channel, backchannel);
}

void LossyOQRouter::ReadInputs( )
{
  bool have_flits = _ReceiveFlits( );
  bool have_credits = _ReceiveCredits( );
  _active = _active || have_flits || have_credits;
}

void LossyOQRouter::_InternalStep( )
{
  if(!_active) {
    return;
  }

  _InputQueuing( );
  bool activity = !_proc_credits.empty();


  if(!_crossbar_flits.empty())
    _SwitchEvaluate( );

  if(!_crossbar_flits.empty()) {
    _SwitchUpdate( );
    activity = activity || !_crossbar_flits.empty();
  }

  _active = activity;

  _OutputQueuing( );

  _bufferMonitor->cycle( );
  _switchMonitor->cycle( );
}

void LossyOQRouter::WriteOutputs( )
{
  _SendFlits( );
  _SendCredits( );
}


//------------------------------------------------------------------------------
// read inputs
//------------------------------------------------------------------------------

bool LossyOQRouter::_ReceiveFlits( )
{
  bool activity = false;
  for(int input = 0; input < _inputs; ++input) {
    Flit * const f = _input_channels[input]->Receive();
    if(f) {

#ifdef TRACK_FLOWS
      ++_received_flits[f->cl][input];
#endif

      if(f->watch) {
	*gWatchOut << GetSimTime() << " | " << FullName() << " | "
		   << "Received flit " << f->id
		   << " from channel at input " << input
		   << "." << endl;
      }
      _in_queue_flits.insert(make_pair(input, f));

      ++_total_buffer_occupancy;

      activity = true;
    }
  }
  return activity;
}

bool LossyOQRouter::_ReceiveCredits( )
{
  bool activity = false;
  for(int output = 0; output < _outputs; ++output) {
    Credit * const c = _output_credits[output]->Receive();
    if(c) {
      _proc_credits.push_back(make_pair(GetSimTime() + _credit_delay,
					make_pair(c, output)));
      activity = true;
    }
  }
  return activity;
}


//------------------------------------------------------------------------------
// input queuing
//------------------------------------------------------------------------------

void LossyOQRouter::_InputQueuing( )
{
  for(map<int, Flit *>::const_iterator iter = _in_queue_flits.begin();
      iter != _in_queue_flits.end();
      ++iter) {

    int const input = iter->first;
    assert((input >= 0) && (input < _inputs));

    Flit * const f = iter->second;
    assert(f);

    int const vc = f->vc;
    assert((vc >= 0) && (vc < _vcs));

    Buffer * const cur_buf = _buf[input];

    int state = cur_buf->GetState(vc);
    assert(state < VC::VCSTATE_LEN);
    if(f->watch) {
      *gWatchOut << GetSimTime() << " | " << FullName() << " | "
		 << "Adding flit " << f->id
		 << " to VC " << vc
		 << " at input " << input
		 << " (state: " << VC::VCSTATE[state];
      if(cur_buf->Empty(vc)) {
	*gWatchOut << ", empty";
      } else {
	assert(cur_buf->FrontFlit(vc));
	*gWatchOut << ", front: " << cur_buf->FrontFlit(vc)->id;
      }
      *gWatchOut << ")." << endl;
    }



    cur_buf->AddFlit(vc, f);

#ifdef TRACK_FLOWS
    ++_stored_flits[f->cl][input];
    if(f->head) ++_active_packets[f->cl][input];
#endif

    _bufferMonitor->write(input, f) ;

    if(cur_buf->GetState(vc) == VC::idle) {
      //      cout << GetSimTime() << " | input: " << input << ": idle" << endl;
      assert(cur_buf->FrontFlit(vc) == f);
      assert(cur_buf->GetOccupancy(vc) == 1);
      assert(f->head);

      if(f->watch) {
        *gWatchOut << GetSimTime() << " | " << FullName() << " | "
      	     << "Using precomputed lookahead routing information for VC " << vc
      	     << " at input " << input
      	     << " (front: " << f->id
      	     << ")." << endl;
      }

      OutputSet route_set_container;
      _rf( this, f, input, &route_set_container, false );
      set<OutputSet::sSetElement> const & route_set = route_set_container.GetSet();
      assert(route_set.size() == 1);
      OutputSet::sSetElement const & route_set_element = *route_set.begin();
      assert(route_set_element.output_port != -1);
      int output_port = route_set_element.output_port;



      // Drop flit if buffer is full...
      // Dropping flits in the middle of a packet creates lots of problems since
      // it causes incomplete packets.  A much easier approach is to make the
      // drop determination at the packet level: when receiving the head flit.
      // To do this, check the packet size (carried by the head flit) against the
      // remaining buffer space.  If the packet size is larger than the available
      // buffer space, then drop the head flit and all subsequent flits up to and
      // including the tail.

      // WARNING: Since the occupancy check doesn't distinguish VCs, this code
      // assumes there is only ever 1 VC configured.  (There is no easy way to
      // make this support multiple VCs with reserved space because this buffer
      // object does not track occupancy by VC.  That is only tracked by the
      // sender.)
      //    if (f->head && ((f->size > (cur_buf->GetSize() - cur_buf->GetOccupancy())) ||
      //    if (f->head && ((f->size > (_total_buffer_size - _total_buffer_occupancy)) ||
      if (f->head && ((f->size > (_output_buffer_size - _output_buffer_occupancy[output_port])) ||
                      // Also randomly drop packets
                      (RandomFloat() < _random_packet_drop_rate))) {
        // If we received a head flit, then we cannot already be in the process
        // of dropping another packet.
        assert(!_drop_packet_at_input[input]);

        if (f->watch) {
          *gWatchOut << GetSimTime() << " | " << FullName() << " | Dropping incoming head flit: " << f->id
//            cout << GetSimTime() << " | " << FullName() << " | Dropping incoming head flit: " << f->id
                     << ", packet: " << f->pid
                     << ", at input: " << input << ", with dest: " << f->dest << ", head: " << f->head
                     << ", tail: " << f->tail << ", size: " << f->size << ", pkt_seq_num: " << f->packet_seq_num
                     << ", tot_buf_occ: " << _total_buffer_occupancy << endl;
        }

        // If this is not the end of the packet, then set the state to indicate
        // all subsequent flits must also be dropped (until a tail is seen).
        if (!f->tail) {
          _drop_packet_at_input[input] = true;
          cur_buf->SetState(vc, VC::active);
        }

        --_total_buffer_occupancy;

        cur_buf->RemoveFlit(vc);

        // Delete the flit object, since it is either a copy of one held in an
        // OPB, or it is a control flit that is just silently dropped.
        f->Free();

      } else if (_drop_packet_at_input[input]) {

        if (f->watch) {
          *gWatchOut << GetSimTime() << " | " << FullName() << " | Dropping incoming body flit: " << f->id
            //            cout << GetSimTime() << " | " << FullName() << " | 1 Dropping incoming body flit: " << f->id
               << ", packet: " << f->pid
               << ", at input: " << input << ", with dest: " << f->dest << ", head: " << f->head
               << ", tail: " << f->tail << ", pkt_seq_num: " << f->packet_seq_num
               << ", tot_buf_occ: " << _total_buffer_occupancy << endl;
        }

        // When we're done receiving the packet, clear the drop state to set up
        // for the next one.
        if (f->tail) {
          _drop_packet_at_input[input] = false;
          cout << "ERROR: should never get here" << endl;
          exit(1);
        }

        --_total_buffer_occupancy;

        cur_buf->RemoveFlit(vc);

        f->Free();

      } else {

        int const expanded_input = input * _input_speedup + vc % _input_speedup;
        int const expanded_output = output_port * _output_speedup + input % _output_speedup;

        int scheduled_crossbar_exit = -1;
        if (_crossbar_latency != -1) {
          scheduled_crossbar_exit = GetSimTime() + _crossbar_latency;
        }

        _crossbar_flits.push_back(make_pair(scheduled_crossbar_exit, make_pair(f, make_pair(expanded_input, expanded_output))));


        ++_output_buffer_occupancy[output_port];

        // Record the output port that the head flit on this input was slotted into.
        // This will be used by body and tail flits.
        // WARNING!!!!! This mechanism assumes that flits of different messages can
        // never be interleaved!!!
        _last_head_flit_output_port[input] = output_port;

        cur_buf->RemoveFlit(vc);

        // Even if we're dropping the packet, we need to keep the VC active until
        // the packet is fully dropped.
        if (!f->tail) {
          cur_buf->SetState(vc, VC::active);
        }
      }

    } else if((cur_buf->GetState(vc) == VC::active) &&
	            (cur_buf->FrontFlit(vc) == f)) {
      //      cout << GetSimTime() << " | input: " << input << ", active" << endl;
      if (_drop_packet_at_input[input]) {
        if (f->watch) {
          *gWatchOut << GetSimTime() << " | " << FullName() << " | Dropping incoming body flit: " << f->id
            //        cout << GetSimTime() << " | " << FullName() << " | 2 Dropping incoming body flit: " << f->id
             << ", packet: " << f->pid
             << ", at input: " << input << ", with dest: " << f->dest << ", head: " << f->head
             << ", tail: " << f->tail << ", pkt_seq_num: " << f->packet_seq_num
             << ", tot_buf_occ: " << _total_buffer_occupancy << endl;
        }

        // When we're done receiving the packet, clear the drop state to set up
        // for the next one.
        if (f->tail) {
          _drop_packet_at_input[input] = false;
          cur_buf->SetState(vc, VC::idle);
          _last_head_flit_output_port[input] = -1;
        }

        --_total_buffer_occupancy;

        cur_buf->RemoveFlit(vc);

        f->Free();
      } else {
        int const expanded_input = input * _input_speedup + vc % _input_speedup;

        int output_port = _last_head_flit_output_port[input];
        int const expanded_output = output_port * _output_speedup + input % _output_speedup;

        int scheduled_crossbar_exit = -1;
        if (_crossbar_latency != -1) {
          scheduled_crossbar_exit = GetSimTime() + _crossbar_latency;
        }
//if ((f->pid == -1)) {
//cout << GetSimTime() << ": " << Name() << ": Inserting ST-ACK with dest: " << f->dest
//     << " into exp_output: " << expanded_output << endl;
//}

//if (f->id == 652) {
//  cout << "Second 652: dest: " << f->dest << ", input: " << input << ", output: " << output_port << ", ei: " << expanded_input << ", eo: " << expanded_output << endl;
//  exit(1);
//}

        _crossbar_flits.push_back(make_pair(scheduled_crossbar_exit, make_pair(f, make_pair(expanded_input, expanded_output))));
        ++_output_buffer_occupancy[output_port];

        cur_buf->RemoveFlit(vc);
        if (f->tail) {
          cur_buf->SetState(vc, VC::idle);
          _last_head_flit_output_port[input] = -1;
        }
      }
    }
  }
  _in_queue_flits.clear();

  while(!_proc_credits.empty()) {

    pair<int, pair<Credit *, int> > const & item = _proc_credits.front();

    int const time = item.first;
    if(GetSimTime() < time) {
      break;
    }

    Credit * const c = item.second.first;
    assert(c);

    int const output = item.second.second;
    assert((output >= 0) && (output < _outputs));

    BufferState * const dest_buf = _next_buf[output];

    if (_use_endpoint_crediting) {
      dest_buf->ProcessCredit(c);
    }
    c->Free();
    _proc_credits.pop_front();
  }
}



//------------------------------------------------------------------------------
// switch traversal
//------------------------------------------------------------------------------

void LossyOQRouter::_SwitchEvaluate( )
{
  for(deque<pair<int, pair<Flit *, pair<int, int> > > >::iterator iter = _crossbar_flits.begin();
      iter != _crossbar_flits.end();
      ++iter) {

    int const time = iter->first;
    if(time >= 0) {
      break;
    }
    iter->first = GetSimTime() + _crossbar_delay - 1;

    Flit const * const f = iter->second.first;
    assert(f);

    int const expanded_input = iter->second.second.first;
    int const expanded_output = iter->second.second.second;


//cout << GetSimTime() << ": SwitchEvaluate: Changing crossbar eject time for flit " << f->id << " to: " << iter->first << endl;

    if(f->watch) {
      *gWatchOut << GetSimTime() << " | " << FullName() << " | "
		 << "Beginning crossbar traversal for flit " << f->id
		 << " from input " << (expanded_input / _input_speedup)
		 << "." << (expanded_input % _input_speedup)
		 << " to output " << (expanded_output / _output_speedup)
		 << "." << (expanded_output % _output_speedup)
		 << "." << endl;
    }
  }
}

void LossyOQRouter::_SwitchUpdate( )
{
  while(!_crossbar_flits.empty()) {

    pair<int, pair<Flit *, pair<int, int> > > const & item = _crossbar_flits.front();

    int const time = item.first;
    if((time < 0) || (GetSimTime() < time)) {
      break;
    }
    assert(GetSimTime() == time);

    Flit * const f = item.second.first;
    assert(f);
//cout << GetSimTime() << ": Flit " << f->id << " with dest: " << f->dest << " finished crossing.  timestamp: " << time << endl;

    int const expanded_input = item.second.second.first;
    int const input = expanded_input / _input_speedup;
    assert((input >= 0) && (input < _inputs));
    int const expanded_output = item.second.second.second;
    int const output = expanded_output / _output_speedup;
    assert((output >= 0) && (output < _outputs));

    if(f->watch) {
      *gWatchOut << GetSimTime() << " | " << FullName() << " | "
		 << "Completed crossbar traversal for flit " << f->id
		 << " from input " << input
		 << "." << (expanded_input % _input_speedup)
		 << " to output " << output
		 << "." << (expanded_output % _output_speedup)
		 << "." << endl;
    }
    _switchMonitor->traversal(input, output, f) ;

    if(f->watch) {
      *gWatchOut << GetSimTime() << " | " << FullName() << " | "
//      cout << GetSimTime() << " | " << FullName() << " | "
		 << "Buffering flit " << f->id
     << " from " << f->src
     << " dest " << f->dest
     << " seq_num: " << f->packet_seq_num
     << " head: " << f->head
     << " tail: " << f->tail
     << " size: " << f->size
		 << " at output " << output
		 << "." << endl;
    }


// Push the flit into the right place into the staging array.
// Since packets are contiguous, we only need to look at the input port.
// iterate through the array to see if there is currently a packet fill in progress from this input port.
//   if not, then we add this packet to the end of the array.

//cout << GetSimTime() << ": Output " << output << ", input " << input << " writing " << f->id << ", input streaming: " << _input_insertion_pointing_at_output_buffer_head[output] << endl;

//cout << GetSimTime() << ": " << Name() << ": 1 output buffer " << output << " size: " << _output_buffer[output].size()
//     << ", empty: " << _output_buffer[output].empty() << endl;



// We always leave the iter at the position of the last insertion, except if it
// just finished a packet, in which case, we move it to the end.
// So always pre-increment the iterator before inserting, unless it is at the
// end.
//if (_oq_insertion_iters[output][input] != _output_buffer[output].end()) {
//
//}

// If wer're streaming from the front, insert at the front, but don't set the pointer there
// because the output function will pop it and invalidate the iter.
    if (_input_insertion_pointing_at_output_buffer_head[output] == input) {
      _output_buffer[output].push_front(f);
      if (f->tail) {
        _input_insertion_pointing_at_output_buffer_head[output] = -1;
        _oq_insertion_iters[output][input] = _output_buffer[output].end();
      }

    } else {

      if (_output_buffer[output].empty()) {
        _output_buffer[output].push_front(f);

        // If there isn't another packet streaming from the front, then claim it
        if ((_input_insertion_pointing_at_output_buffer_head[output] == -1) &&
            (!f->tail)) {
          _input_insertion_pointing_at_output_buffer_head[output] = input;
          // Move the iter out of the way so it doesn't get invalidated
          _oq_insertion_iters[output][input] = _output_buffer[output].end();
        }
        // Though we inserted at the front, another packet is streaming from the
        // front (and we expect it to fill in a flit later in this cycle), so we
        // need to set the _oq_insertion_iter appropriately.  Otherwise, the packet
        // will not be contiguous in the output buffer.
        else if ((_input_insertion_pointing_at_output_buffer_head[output] != -1) &&
                 (!f->tail)) {
          _oq_insertion_iters[output][input] = _output_buffer[output].begin();
        }
        // Otherwise this is a tail flit, so move the iter to the end since the
        // packet is complete.
        else {
          _oq_insertion_iters[output][input] = _output_buffer[output].end();
        }

      } else {

        if (_oq_insertion_iters[output][input] != _output_buffer[output].end()) {
          _oq_insertion_iters[output][input]++;
        }

        list< Flit * >::iterator tmp = _output_buffer[output].insert(_oq_insertion_iters[output][input], f);
        // If this is the end of the packet, then move the pointer to the end.
        if (f->tail) {
          _oq_insertion_iters[output][input] = _output_buffer[output].end();
        } else {
          _oq_insertion_iters[output][input] = tmp;
        }
      }
    }



/*
if ((output == 1) && (GetSimTime() >= 120000)) {
cout << GetSimTime() << ": Output " << output << ", input writing to front: " << _input_insertion_pointing_at_output_buffer_head[output] << endl;
for (list< Flit * >::iterator iter = _output_buffer[output].begin();
     iter != _output_buffer[output].end();
     ++iter) {
  cout << "  " << (*iter)->id << ", head: " << (*iter)->head << ", tail: " << (*iter)->tail << ", dest: " << (*iter)->dest << endl;
}
}
*/

//_oq_insertion_iters[output][input] = _output_buffer[output].end();  // no segfault, but interleaved flits

//if (_oq_insertion_iters[output][input] == _output_buffer[output].end()) {
//  _output_buffer[output].push_back(f);
//}



/*
  // Push any new packets (f->head) onto the back
  if ((_output_buffer[output].size() == 0) || (f->head)) {
//    _output_buffer[output].push_back(f);
// _oq_insertion_iters point to the last item inserted

cout << GetSimTime() << ": " << Name() << ": output buffer: " << output << " is empty or flit is head." << endl;

    _oq_insertion_iters[output][input] = _output_buffer[output].insert(_oq_insertion_iters[output][input], f);

  // Otherwise, we have a non-head flit, and have to insert it into the right place
  } else {

    // _oq_insertion_iters point to the last item inserted.
// If we just sent the last flit we had in the buffer for this packet, but there are more coming
// across the crossbar that we need to insert, then we need to insert it at the head of the buffer,
// and not behind an older packet.
//if ((_oq_insertion_iters[output][input] == _output_buffer[output].begin()) &&
//    (f->pid != _output_buffer[output].front()->pid)) {
//}
if ((!((_input_insertion_pointing_at_output_buffer_head[output] == input) &&
     (f->pid != _output_buffer[output].front()->pid))) &&
   (_oq_insertion_iters[output][input] != _output_buffer[output].end())) {
  ++_oq_insertion_iters[output][input];
}
    _oq_insertion_iters[output][input] = _output_buffer[output].insert(_oq_insertion_iters[output][input], f);

    if (_oq_insertion_iters[output][input] == _output_buffer[output].begin()) {
      _input_insertion_pointing_at_output_buffer_head[output] = input;
    } else {
      _input_insertion_pointing_at_output_buffer_head[output] = -1;
    }

    if (f->tail) {
      _oq_insertion_iters[output][input] = _output_buffer[output].end();
    }

    // WARNING!!!
    // Flits are guaranteed to arrive contiguously, but there is a possibility
    // that the last flit received has already been sent out...
    // In that case, the iterator becomes invalidated...
    // If that happens, we need to do something with the iterator instead of
    // letting it get invalidated.

  }
*/



/*
  // Also push any new packets (f->head) onto the back
  if ((_output_buffer[output].empty()) || (f->head)) {
    _output_buffer[output].push_back(f);

// one corner-case:  the packet currently streaming out has no flits left in the output buffer,
// but still has flits coming in from the input port.  These new flits should get placed into the
// front of the buffer, since packets must be contiguous.
//  At the same time, we could have flits being filled in from other input ports due to random
// ordering of insertion into _crossbar_flits.
} else if ((_current_pid_output_in_progress[output] != -1) && (_current_pid_output_in_progress[output] == f->pid) &&
    (_output_buffer[output].front()->pid != f->pid)) {
  _output_buffer[output].push_front(f);


  // Otherwise, we have a non-head flit, and have to insert it into the right place
  } else {
    bool found_packet_loop_til_end = false;
    bool inserted = false;
    for(list< Flit * >::const_iterator iter = _output_buffer[output].begin();
        iter != _output_buffer[output].end();
        ++iter) {

      if (found_packet_loop_til_end) {
  // If we get to the next packet, or the end of the list, then insert (in before the next packet)
        if ((*iter)->head) {
          _output_buffer[output].insert(iter, f);
          inserted = true;
          break;
        }
      }

  // if the iterator is at a head flit, check if it belongs to the same packet.
  // if so, keep looking until we find the end of the packet, and append the new flit.
      else if ((*iter)->pid == f->pid) {
        found_packet_loop_til_end = true;
      }
    }
    // If we got to the end and haven't inserted, then append to the very end
    if (!inserted) {
      _output_buffer[output].push_back(f);
    }
  }
*/


//    _output_buffer[output].push(f);
    //the output buffer size isn't precise due to flits in flight
    //but there is a maximum bound based on output speed up and ST traversal
//    assert(_output_buffer[output].size()<=(size_t)_output_buffer_size+ _crossbar_delay* _output_speedup+( _output_speedup-1) ||_output_buffer_size==-1);


// FIXME: TNL 091721: Comment out for speed...
//   The .size function iterates through every element in the list.  Commenting
//   this out results in a 5x improvement in simulation speed.  However, we
//   still need this check, so we will have to track the size on the side (in
//   a later commit).
//
//    if (!(_output_buffer[output].size()<=(size_t)_output_buffer_size+ _crossbar_delay* _output_speedup+( _output_speedup-1) ||_output_buffer_size==-1)) {
//  cout << GetSimTime() << ": " << Name() << " Buffer size assertion failed.  Size: " << _output_buffer[output].size() << endl;
//  assert(0);
//}

//cout << GetSimTime() << ": Popping _crossbar_flits.  size: " << _crossbar_flits.size() << endl;
    _crossbar_flits.pop_front();
  }
}


//------------------------------------------------------------------------------
// output queuing
//------------------------------------------------------------------------------

void LossyOQRouter::_OutputQueuing( )
{
  for(map<int, Credit *>::const_iterator iter = _out_queue_credits.begin();
      iter != _out_queue_credits.end();
      ++iter) {

    int const input = iter->first;
    assert((input >= 0) && (input < _inputs));

    Credit * const c = iter->second;
    assert(c);
    assert(!c->vc.empty());

    _credit_buffer[input].push(c);
  }
  _out_queue_credits.clear();
}

//------------------------------------------------------------------------------
// write outputs
//------------------------------------------------------------------------------

void LossyOQRouter::_SendFlits( )
{
  for ( int output = 0; output < _outputs; ++output ) {
    if ( !_output_buffer[output].empty( ) ) {
      Flit * const f = _output_buffer[output].front( );
      assert(f);



// Don't pop the front of the list if there is an iterator pointing at it, because that
// will invalidate the iterator, and weird things start happening...
// like the output buffer will be not empty, but have a size of 0...

// start by moving it to the end.
//  if ((_input_insertion_pointing_at_output_buffer_head[output] != -1)) {
//    _oq_insertion_iters[output][_input_insertion_pointing_at_output_buffer_head[output]] = _output_buffer[output].end();
//  }

//cout << GetSimTime() << ": " << Name() << ": 2 output buffer " << output << " size: " << _output_buffer[output].size()
//     << ", empty: " << _output_buffer[output].empty() << endl;

if (_input_insertion_pointing_at_output_buffer_head[output] != -1) {
  if (_oq_insertion_iters[output][_input_insertion_pointing_at_output_buffer_head[output]] == _output_buffer[output].begin()) {
    if (f->tail) {
      _oq_insertion_iters[output][_input_insertion_pointing_at_output_buffer_head[output]] = _output_buffer[output].end();
//      _input_insertion_pointing_at_output_buffer_head[output] = -1;
    } else {
      ++_oq_insertion_iters[output][_input_insertion_pointing_at_output_buffer_head[output]];
    }
  }
}

  _output_buffer[output].pop_front( );

  --_total_buffer_occupancy;


  --_output_buffer_occupancy[output];

//  if ((!f->tail) && (!_output_buffer[output].empty())) {
//    _oq_insertion_iters[output][_input_insertion_pointing_at_output_buffer_head[output]] = _output_buffer[output].begin();
//  }

//cout << GetSimTime() << ": " << Name() << ": 3 output buffer " << output << " size: " << _output_buffer[output].size()
//     << ", empty: " << _output_buffer[output].empty() << endl;

/*
// If the input insertion iter was invalidated, move it back to the beginning...
// except if there is another packet... then we're hosed.
if ((!f->tail) && (_input_insertion_pointing_at_output_buffer_head[output] != -1)) {
  _oq_insertion_iters[output][_input_insertion_pointing_at_output_buffer_head[output]] = _output_buffer[output].begin();
// If the packet is done, move the pointer to the back
} else if ((f->tail) && (_input_insertion_pointing_at_output_buffer_head[output] != -1)) {
  _oq_insertion_iters[output][_input_insertion_pointing_at_output_buffer_head[output]] = _output_buffer[output].end();
  _input_insertion_pointing_at_output_buffer_head[output] = -1;
}
*/




      if (f->head) {
        _current_pid_output_in_progress[output] = f->pid;
      }
      if (f->tail) {
        _current_pid_output_in_progress[output] = -1;
      }

#ifdef TRACK_FLOWS
      ++_sent_flits[f->cl][output];
#endif

      if(f->watch)
	*gWatchOut << GetSimTime() << " | " << FullName() << " | "
//	cout << GetSimTime() << " | " << FullName() << " | "
		    << "Sending flit " << f->id
		    << " to channel at output " << output
		    << "." << endl;
      if(gTrace) {
	cout << "Outport " << output << endl << "Stop Mark" << endl;
      }
      _output_channels[output]->Send( f );
    }
  }
}

void LossyOQRouter::_SendCredits( )
{
  for ( int input = 0; input < _inputs; ++input ) {
    if ( !_credit_buffer[input].empty( ) ) {
      Credit * const c = _credit_buffer[input].front( );
      assert(c);
      _credit_buffer[input].pop( );
      _input_credits[input]->Send( c );
    }
  }
}


//------------------------------------------------------------------------------
// misc.
//------------------------------------------------------------------------------

void LossyOQRouter::Display( ostream & os ) const
{
  for ( int input = 0; input < _inputs; ++input ) {
    _buf[input]->Display( os );
  }
}

int LossyOQRouter::GetUsedCredit(int o) const
{
  assert((o >= 0) && (o < _outputs));
  BufferState const * const dest_buf = _next_buf[o];
  return dest_buf->Occupancy();
}

int LossyOQRouter::GetBufferOccupancy(int i) const {
  assert(i >= 0 && i < _inputs);
  return _buf[i]->GetOccupancy();
}

#ifdef TRACK_BUFFERS
int LossyOQRouter::GetUsedCreditForClass(int output, int cl) const
{
  assert((output >= 0) && (output < _outputs));
  BufferState const * const dest_buf = _next_buf[output];
  return dest_buf->OccupancyForClass(cl);
}

int LossyOQRouter::GetBufferOccupancyForClass(int input, int cl) const
{
  assert((input >= 0) && (input < _inputs));
  return _buf[input]->GetOccupancyForClass(cl);
}
#endif

vector<int> LossyOQRouter::UsedCredits() const
{
  vector<int> result(_outputs*_vcs);
  for(int o = 0; o < _outputs; ++o) {
    for(int v = 0; v < _vcs; ++v) {
      result[o*_vcs+v] = _next_buf[o]->OccupancyFor(v);
    }
  }
  return result;
}

vector<int> LossyOQRouter::FreeCredits() const
{
  vector<int> result(_outputs*_vcs);
  for(int o = 0; o < _outputs; ++o) {
    for(int v = 0; v < _vcs; ++v) {
      result[o*_vcs+v] = _next_buf[o]->AvailableFor(v);
    }
  }
  return result;
}

vector<int> LossyOQRouter::MaxCredits() const
{
  vector<int> result(_outputs*_vcs);
  for(int o = 0; o < _outputs; ++o) {
    for(int v = 0; v < _vcs; ++v) {
      result[o*_vcs+v] = _next_buf[o]->LimitFor(v);
    }
  }
  return result;
}
