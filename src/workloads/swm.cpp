/*
 * swm.cpp
 *
 * Booksim scalable workload model implementation
 *
 *  Created on: Jun 9, 2022
 *      Author: cjbeckma
 */
#include <swm.hpp>

//
// the SWM thread class implementation
//
//
//
//
//
std::map<WorkloadMessage::msg_t, std::string> type_to_string = {
   {WorkloadMessage::DummyRequest, "DummyRequest"},
   {WorkloadMessage::GetRequest, "GetRequest"},
   {WorkloadMessage::NbGetRequest, "NbGetRequest"},
   {WorkloadMessage::PutRequest, "PutRequest"},
   {WorkloadMessage::SendRequest, "SendRequest"},
   {WorkloadMessage::RecvRequest, "RecvRequest"}
};

static int clear_stats_cnt = 0;
// Notify booksim to clear the stats
void SwmThread::clear_stats()
{
   if(_track_acks)
      quiet();

   clear_stats_cnt++;
   if(clear_stats_cnt == _np) {
      gClearStats = true;
      clear_stats_cnt = 0;
   }
}

// after derived class is initialized it is safe to call this to start coroutine
void SwmThread::start()
{
   _coro = new coro_t(std::bind(&SwmThread::wrapper, this, std::placeholders::_1));
}

// SWM INTERFACE

// simulate some number of cycles of local work
void SwmThread::work(cycle_t cycles)
{
    if (!gSimEnabled){
        return;
    }
   _state = ready;
   _time += cycles;
}

// generate a PUT message
void SwmThread::put(int size, int dest)
{
    if (!gSimEnabled){
        return;
    }
      _state = message;
      _put++;
      _time += _put_overhead;
      DBGPRINT("PUT to " << dest);
      (*_sink)({WorkloadMessage::PutRequest, 0, _me, dest, size, false});
}

// generate a GET request message
void SwmThread::get(int size, int dest)
{
    if (!gSimEnabled){
        return;
    }
      _state = message;
      _get++;
      _time += _get_overhead;
      DBGPRINT("GET from " << dest);
      (*_sink)({WorkloadMessage::GetRequest, 0, _me, dest, size, false});
}

// generate a non-blocking GET request message
void SwmThread::getnb(int size, int dest)
{
   _state = message;
   _get++;
   _time += _get_overhead;
   DBGPRINT("NBGET from " << dest);
   (*_sink)({WorkloadMessage::NbGetRequest, 0, _me, dest, size, false});
}

// generate a SEND message
void SwmThread::send(int size, int dest)
{
   _state = message;
   _send++;
   //_time += PUT_OVHD;
   DBGPRINT("SEND to " << dest);
   _time += _send_overhead;
   (*_sink)({WorkloadMessage::SendRequest, 0, _me, dest, size, false});
}

// RECV a message
void SwmThread::recv(int src)
{
   // first see if it matches any previously received messages
   for (auto i = _recvd.begin(); i != _recvd.end(); ++i)
   {
      DBGPRINT("RECV from " << i->source);
      if (i->source == src) {
         _recvd.erase(i);
         return;
      }
   }
   // if not, put myself into wait state and yield a bogus message
   _state = wait;
   DBGPRINT("RECV wait for data to arrive state is wait");
   _time += _recv_overhead;
   (*_sink)({WorkloadMessage::RecvRequest, 0, _me, src, 0/*bogus size*/, 0});
}

// wait for non-blocking get replies and acks
void SwmThread::quiet()
{
   while(!_outstanding_acks.empty()) {
      _state = quiet_wait;
      auto i = _outstanding_acks.front();
      DBGPRINT("outstanding acks " << _outstanding_acks.size() << " waiting for reply from " << i.dest);
      (*_sink)({i.type, 0, _me, i.dest, 0/*bogus size*/, 0});
   }
}

// TODO: maybe have another state called "yield"
void SwmThread::thread_yield()
{
   // Dummy request to yield to the other threads
   _state = message;
   (*_sink)({WorkloadMessage::DummyRequest, 0, _me, _me, 0/*bogus size*/, 0});
}

void SwmThread::SwmMarker(int marker)
{
   _state = message;
   (*_sink)({WorkloadMessage::DummyRequest, marker, _me, _me, 0/*bogus size*/, 0});
}

void SwmThread::SwmEndMarker(int marker)
{
   _state = message;
   (*_sink)({WorkloadMessage::DummyRequest, marker, _me, _me, 0/*bogus size*/, 0});
}


// GENERATOR INTERFACE

// unconditionally run the model to generate the next packet
   inline
void SwmThread::go(cycle_t now)
{
   _state = ready;
   _time = now;
   (*_coro)(); // resume the coroutine
   if (! *_coro) {
      _state = done;
   }
}

// update state and maybe advance the model execution
void SwmThread::next(cycle_t now)
{
   if (_state == message && _coro->get().type == WorkloadMessage::GetRequest){
      _state = wait;
   }
   else if (_state != wait) {
      bool is_dummy = _coro->get().type == WorkloadMessage::DummyRequest;
      if(_track_acks && !is_dummy)
         _outstanding_acks.push_back(_coro->get());

      if(!is_done())
         go(now);
   }
}


// is the model process waiting for a network reply?
inline
bool SwmThread::is_waiting(const pkt_s &m) const
{
   if (_state != wait) { return false; }
   auto w = _coro->get();
   return (m.type == WorkloadMessage::GetRequest && w.dest == m.source);
}


// provide a network reply, and advance model execution
void SwmThread::reply(cycle_t now, const pkt_s &reply)
{
   DBGPRINT("got " << (reply.type == WorkloadMessage::GetRequest ? "GET" : reply.type == WorkloadMessage::SendRequest ? "SEND":"PUT") << " reply from " << reply.source);
   _reply++;
   if (is_waiting(reply))  {
      for (auto i = _outstanding_acks.begin(); i != _outstanding_acks.end(); ++i) {
         if (i->dest == reply.source) {
            _outstanding_acks.erase(i);
            break;
         }
      }
      go(now);
   }
   else { // non-blocking get replies that arrive before fence
      for (auto i = _outstanding_acks.begin(); i != _outstanding_acks.end(); ++i) {
         if (i->dest == reply.source) {
            _outstanding_acks.erase(i);
            if(_state == quiet_wait) {
               go(now);
            }
            break;
         }
      }
   }
}

// provide a network SEND, and advance model execution
void SwmThread::sendin(cycle_t now, const pkt_s &sent)
{
   DBGPRINT("got SEND from " << sent.source << " at time " << now);
   //
   bool is_matched = _coro->get().type == WorkloadMessage::RecvRequest && _coro->get().dest == sent.source;
   if(_state == wait && is_matched) {
     go(now);
   }
   else {
      DBGPRINT("unmatched");
      _recvd.push_back(sent);
   }
}
