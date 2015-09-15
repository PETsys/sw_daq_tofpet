#ifndef __nDAQ__EVENT_HPP__DEFINED__
#define __nDAQ__EVENT_HPP__DEFINED__

#include <cstddef>
#include <sys/types.h>
#include <stdint.h>

namespace DAQ { namespace Core {

	inline long long tAbs(long long x) { return x >= 0 ? x : - x; };

	struct RawPulse {
		enum Type { TOFPET, STIC, DSIPM };
	  
		unsigned short T;
		long long time;
		long long timeEnd;
		short region;
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

		RawPulse() {
			time = -1;
			timeEnd = -1;
			channelID = -1;
			region = -1;
			channelIdleTime = 0;
		};
	};

	struct Pulse {
		RawPulse *raw;
		long long time;
		long long timeEnd;
		short region;
		
		int channelID;
		float energy;
		
		bool badEvent;
		float tofpet_TQT;
		float tofpet_TQE;


		
		Pulse() {
			time = -1;
			timeEnd = -1;
			channelID = -1;
			energy = 0;
			region = -1;
			raw = NULL;
			badEvent = true;
		};
	};

	struct RawHit {
		Pulse *bottom;
		Pulse *top;
		long long time;
		short region;
		int crystalID;
		float energy;
		float asymmetry;
		float missingEnergy;
		short nMissing;

		RawHit() {
			time = -1;
			region = -1;
			crystalID = -1;
			energy = 0;
			asymmetry = 0;
			missingEnergy = 0;
			nMissing = 0;
			bottom = NULL;
			top = NULL;
		};
	};

	struct Hit {
		RawHit *raw;
		long long time;
		short region;
		float x;
		float y;
		float z;
		int xi;
		int yi;
		float energy;
		float missingEnergy;
		short nMissing;

		Hit() {
			time = -1;
			region = -1;
			x = 0;
			y = 0;
			z = 0;
			xi = -1;
			yi = -1;
			energy = 0;
			missingEnergy = 0;
			nMissing = 0;
			raw = NULL;
		};
	};

	struct GammaPhoton {
		static const int maxHits = 16;
		long long time;
		short region;
		float energy;
		float x, y, z;
		float missingEnergy;
		short nMissing;
		int nHits;
		Hit *hits[maxHits];		

		GammaPhoton() {
			time = -1;
			region = -1;
			energy = 0;
			missingEnergy = 0;
			nMissing = 0;
			x = y = z = 0;
			nHits = 0;			
			for(int i = 0; i < maxHits; i++)
				hits[i] = NULL;
		};
	};

	struct Coincidence {
		static const int maxPhotons = 2;
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
