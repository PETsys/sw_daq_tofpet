#ifndef __DAQ_CORE_EVENTBUFFER_HPP__DEFINED__
#define __DAQ_CORE_EVENTBUFFER_HPP__DEFINED__
#include "Event.hpp"
#include <stdlib.h>

namespace DAQ { namespace Core {

	template <class TEvent>
	class EventBuffer {
	public:

		EventBuffer(unsigned initialCapacity) {
			initialCapacity = ((initialCapacity / 1024) + 1) * 1024;				
			buffer = (TEvent *)malloc(sizeof(TEvent)*initialCapacity);
			capacity = initialCapacity;
			used = 0;
			tMin = -1;
			tMax = -1;
		};
		
		void reserve(unsigned newCapacity) {
			if (newCapacity <= capacity) 
				return;
			
			TEvent * reBuffer = (TEvent *)realloc((void*)buffer, sizeof(TEvent)*newCapacity);
			buffer = reBuffer;
			capacity = newCapacity;			
		};

		TEvent &getWriteSlot() {
			if(used >= capacity) {
				size_t increment = ((capacity / 10240) + 1) * 1024;
				capacity += increment;				
				TEvent * reBuffer = (TEvent *)realloc((void*)buffer, sizeof(TEvent)*capacity);
				buffer = reBuffer;
			}
			return buffer[used];	
		};

		void pushWriteSlot() {
			used++;
		};

		void push(TEvent &e) {
			getWriteSlot() = e;
			pushWriteSlot();
		};

		TEvent & get(size_t index) {
			return buffer[index];
		};

		TEvent & getLast() {
			return get(getSize()-1);
		};

		size_t getSize() {
			return used;
		};


		~EventBuffer() {
			free((void*)buffer);
		};
		
		void setTMin(long long t) {
			tMin = t;
		}
		
		void setTMax(long long t) {
			tMax = t;
		}
		
		long long getTMin() {
			return tMin;
		}
		
		long long getTMax() {
			return tMax;
		}

	private:
		TEvent *buffer;
		size_t capacity;
		size_t used;
		
		long long tMin;
		long long tMax;
		
	};

}}
#endif
