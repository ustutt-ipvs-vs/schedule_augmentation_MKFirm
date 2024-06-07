#pragma once

namespace tsndgm
{
    typedef unsigned int FrameSize;
    typedef unsigned int StreamID;
    typedef unsigned long BurstSize;
    typedef unsigned int DeviceId;
    typedef long Tick;
    typedef Tick Delay;
    typedef unsigned long DataRate;

    typedef std::pair<DeviceId, DeviceId> Edge;
} // namespace tsndgm
