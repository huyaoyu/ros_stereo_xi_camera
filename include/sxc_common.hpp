#ifndef __SXC_COMMON_HPP__
#define __SXC_COMMON_HPP__

#define DB_FACTOR (10)

namespace sxc
{
    
typedef float xf;

/***
 * dB is defined as 10*log10(G).
 */
xf dBToGain(xf dB);
xf GainToDB(xf G);

}

#endif // __SXC_COMMON_HPP__
