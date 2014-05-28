#ifndef __DAQ__COMMON__INSTRUMENTATION_H__DEFINED__
#define __DAQ__COMMON__INSTRUMENTATION_H__DEFINED__

#include <sys/types.h>
namespace DAQ { namespace Common {
	
	void atomicIncrement(volatile u_int32_t &val);
	void atomicAdd(volatile u_int32_t &val, u_int32_t increment);
	
}}

#endif