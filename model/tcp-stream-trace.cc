#include "ns3/log.h"
#include "ns3/trace-helper.h"
#include "ns3/simulator.h"

namespace ns3 {

void
CwndChange(Ptr<OutputStreamWrapper> stream,
           uint32_t oldCwnd,
           uint32_t newCwnd)
{
  *stream->GetStream() << Simulator::Now().GetSeconds()
                       << "\t" << oldCwnd
                       << "\t" << newCwnd
                       << std::endl;
}

} // namespace ns3
