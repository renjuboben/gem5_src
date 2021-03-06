/*
 * Copyright (c) 1999-2008 Mark D. Hill and David A. Wood
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Unordered buffer of messages that can be inserted such
 * that they can be dequeued after a given delta time has expired.
 */

#ifndef __MEM_RUBY_BUFFERS_MESSAGEBUFFER_HH__
#define __MEM_RUBY_BUFFERS_MESSAGEBUFFER_HH__

#include <algorithm>
#include <cassert>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include "debug/RubyQueue.hh"
#include "mem/ruby/common/Address.hh"
#include "mem/ruby/common/Consumer.hh"
#include "mem/ruby/slicc_interface/Message.hh"
#include "mem/packet.hh"
#include "params/MessageBuffer.hh"
#include "sim/sim_object.hh"

class MessageBuffer : public SimObject
{
  public:
    typedef MessageBufferParams Params;
    MessageBuffer(const Params *p);

    void reanalyzeMessages(Addr addr);
    void reanalyzeAllMessages();
    void stallMessage(Addr addr);

    // TRUE if head of queue timestamp <= SystemTime
    bool isReady() const;

    void
    delayHead()
    {
        MsgPtr m = m_prio_heap.front();
        std::pop_heap(m_prio_heap.begin(), m_prio_heap.end(),
                      std::greater<MsgPtr>());
        m_prio_heap.pop_back();
        enqueue(m, Cycles(1));
    }

    bool areNSlotsAvailable(unsigned int n);
    int getPriority() { return m_priority_rank; }
    void setPriority(int rank) { m_priority_rank = rank; }
    void setConsumer(Consumer* consumer)
    {
        DPRINTF(RubyQueue, "Setting consumer: %s\n", *consumer);
        if (m_consumer != NULL) {
            fatal("Trying to connect %s to MessageBuffer %s. \
                  \n%s already connected. Check the cntrl_id's.\n",
                  *consumer, *this, *m_consumer);
        }
        m_consumer = consumer;
    }

    void setSender(ClockedObject* obj)
    {
        DPRINTF(RubyQueue, "Setting sender: %s\n", obj->name());
        assert(m_sender == NULL || m_sender == obj);
        m_sender = obj;
    }

    void setReceiver(ClockedObject* obj)
    {
        DPRINTF(RubyQueue, "Setting receiver: %s\n", obj->name());
        assert(m_receiver == NULL || m_receiver == obj);
        m_receiver = obj;
    }

    Consumer* getConsumer() { return m_consumer; }

    bool getOrdered() { return m_strict_fifo; }

    //! Function for extracting the message at the head of the
    //! message queue.  The function assumes that the queue is nonempty.
    const Message* peek() const;

    const MsgPtr&
    peekMsgPtr() const
    {
        assert(isReady());
        return m_prio_heap.front();
    }

    void enqueue(MsgPtr message) { enqueue(message, Cycles(1)); }
    void enqueue(MsgPtr message, Cycles delta);

    //! Updates the delay cycles of the message at the head of the queue,
    //! removes it from the queue and returns its total delay.
    Cycles dequeue();

    void recycle();
    bool isEmpty() const { return m_prio_heap.size() == 0; }
    bool isStallMapEmpty() { return m_stall_msg_map.size() == 0; }
    unsigned int getStallMapSize() { return m_stall_msg_map.size(); }

    unsigned int getSize();

    void clear();
    void print(std::ostream& out) const;
    void clearStats() { m_not_avail_count = 0; m_msg_counter = 0; }

    void setIncomingLink(int link_id) { m_input_link_id = link_id; }
    void setVnet(int net) { m_vnet_id = net; }

    // Function for figuring out if any of the messages in the buffer need
    // to be updated with the data from the packet.
    // Return value indicates the number of messages that were updated.
    // This required for debugging the code.
    uint32_t functionalWrite(Packet *pkt);

  private:
    //added by SS
    const Cycles m_recycle_latency;

    void reanalyzeList(std::list<MsgPtr> &, Tick);

  private:
    // Data Members (m_ prefix)
    //! The two ends of the buffer.
    ClockedObject* m_sender;
    ClockedObject* m_receiver;

    //! Consumer to signal a wakeup(), can be NULL
    Consumer* m_consumer;
    std::vector<MsgPtr> m_prio_heap;

    // use a std::map for the stalled messages as this container is
    // sorted and ensures a well-defined iteration order
    typedef std::map<Addr, std::list<MsgPtr> > StallMsgMapType;

    StallMsgMapType m_stall_msg_map;

    const unsigned int m_max_size;
    Cycles m_time_last_time_size_checked;
    unsigned int m_size_last_time_size_checked;

    // variables used so enqueues appear to happen immediately, while
    // pop happen the next cycle
    Cycles m_time_last_time_enqueue;
    Tick m_time_last_time_pop;
    Tick m_last_arrival_time;

    unsigned int m_size_at_cycle_start;
    unsigned int m_msgs_this_cycle;

    int m_not_avail_count;  // count the # of times I didn't have N
                            // slots available
    uint64_t m_msg_counter;
    int m_priority_rank;
    const bool m_strict_fifo;
    const bool m_randomization;

    int m_input_link_id;
    int m_vnet_id;
};

Cycles random_time();

inline std::ostream&
operator<<(std::ostream& out, const MessageBuffer& obj)
{
    obj.print(out);
    out << std::flush;
    return out;
}

#endif // __MEM_RUBY_BUFFERS_MESSAGEBUFFER_HH__
