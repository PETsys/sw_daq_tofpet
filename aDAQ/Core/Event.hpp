#ifndef __nDAQ__EVENT_HPP__DEFINED__
#define __nDAQ__EVENT_HPP__DEFINED__

#include <cstddef>
#include <sys/types.h>
#include <stdint.h>

namespace DAQ { namespace Core {

	inline long long tAbs(long long x) { return x >= 0 ? x : - x; };

	struct RawHit {
		enum Type { TOFPET, STIC, DSIPM };
	  
		long long time;
		long long timeEnd;
		int channelID;
		long long channelIdleTime;

		Type feType;

		union {
		  struct {
			  short tcoarse;
			  short ecoarse;
			  short tfine;
			  short efine;
			  short tac;
			  long long tacIdleTime;
		  } tofpet;
		  struct {
			  short tcoarse;
			  short ecoarse;
			  short tfine;
			  short efine;
			  bool tBadHit;
			  bool eBadHit;
			  // Write here any other STIC auxiliary fields that may be relevant for data analysis;
		  } stic;
		  struct {
		  } dsipm;
		} d;

		RawHit() {
			time = -1;
		};
	};

	struct Hit {
		RawHit *raw;
		long long time;
		long long timeEnd;
		float energy;

		short region;
		short xi;
		short yi;
		float x;
		float y;
		float z;
		
		bool badEvent;
		float tofpet_TQT;
		float tofpet_TQE;


		
		Hit() {
			time = -1;
			raw = NULL;
			badEvent = true;
		};
	};

	struct GammaPhoton {
		static const int maxHits = 16;
		long long time;
		float energy;
		short region;
		float x, y, z;
		int nHits;
		Hit *hits[maxHits];		

		GammaPhoton() {
			time = -1;
			for(int i = 0; i < maxHits; i++)
				hits[i] = NULL;
		};
	};

	struct Coincidence {
		static const int maxPhotons = 2;
		long long time;
		int nPhotons;
		GammaPhoton *photons[maxPhotons];
		
		Coincidence() {
			nPhotons = 0;
			for(int i = 0; i < maxPhotons; i++)
				photons[i] = NULL;
		};
	};	
}}
#endif
