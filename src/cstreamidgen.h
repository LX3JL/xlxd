//
//  cstreamidgen.h
//  xlxd
//
//  Global stream ID generator — produces unique 16-bit IDs for protocols
//  that do not provide their own wire stream IDs.
//
// ----------------------------------------------------------------------------
//    This file is part of xlxd.
//
//    xlxd is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    xlxd is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
// ----------------------------------------------------------------------------

#ifndef cstreamidgen_h
#define cstreamidgen_h

#include <atomic>
#include <cstdint>
#include <random>

// Global stream ID generator.
// Produces unique 16-bit stream IDs via an atomic counter seeded randomly
// at startup. Thread-safe, lock-free, portable (x86, ARM).
//
// Zero is skipped — it is used as a "not set" sentinel throughout the codebase.
// Wraps after 65534 IDs (~21 hours at 50 streams/minute), by which time all
// prior streams have long since closed (stream timeout is 1.6 seconds).

class CStreamIdGen
{
public:
    static uint16_t Next(void)
    {
        uint16_t id = Counter().fetch_add(1, std::memory_order_relaxed);
        if ( id == 0 )
            id = Counter().fetch_add(1, std::memory_order_relaxed);
        return id;
    }

private:
    static std::atomic<uint16_t> &Counter(void)
    {
        static std::atomic<uint16_t> s_counter(Seed());
        return s_counter;
    }

    static uint16_t Seed(void)
    {
        try
        {
            std::random_device rd;
            uint16_t seed = static_cast<uint16_t>(rd());
            return (seed != 0) ? seed : 1;
        }
        catch (...)
        {
            return 1;
        }
    }
};

#endif /* cstreamidgen_h */
