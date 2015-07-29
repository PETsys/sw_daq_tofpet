#ifndef __DAQ__TDC__P2_HPP__DEFINED__
#define __DAQ__TDC__P2_HPP__DEFINED__

namespace DAQ { namespace TOFPET {
	
	class P2 {
	public:
		P2(int nChannels);
		~P2();
		
		void setT0(int channel, int tac, bool isT, float t0);
		float getT0(int channel, int tac, bool isT);
		void setShapeParameters(int channel, int tac, bool isT, float tB, float m, float p2);
		void setLeakageParameters(int channel, int tac, bool isT, float tQ, float a0, float a1, float a2);

		float getQ(int channel, int tac, bool isT, int adc, long long tacIdleTime, float coarseToT);
		float getQtac(int channel, int tac, bool isT, int adc, long long tacIdleTime);
		float getQmodulationCorrected(float tQ, int channel, bool isT, float coarseToT);
		bool isNormal(int channel, int tac, bool isT, int adc, int coarse, long long tacIdleTime, float coarseToT);
		float getT(int channel, int tac, bool isT, int adc, int coarse, long long tacIdleTime, float coarseToT);
		float getEnergy(int channel, float tot);
		void loadTDCFile(int start, int end, const char *fileName);
		void loadTQFile(int start, int end, const char *fileName);
		void loadTOTFile(int start, int end, const char *fileName);
		void loadOffsetFile(int start, int end, const char *fileName);
		void storeFile(int start, int end, const char *fileName);
		void loadFiles(const char *mapFileName, bool loadTQ, bool multistep, float step1, float step2 );
		
		void setAll(float v);

		float *nBins_tqT;   
		float *nBins_tqE;  
		
		float *timeOffset;
	private:
		int  nChannels;
		int getIndex(int channel, int tac, bool isT);
		int getIndexTQ(int channel, bool isT, int totbin);
		long getIndexTOT(int channel, float tot);
		struct TAC {			
			float t0;
			struct {
				float tB;
				float m;
				float p2;
			} shape;
			
			struct {
				float tQ;
				float a0;
				float a1;
				float a2;
			} leakage;
			
		};
	  	struct TQ {
			
			int nbins;
			int channel;
			bool isT;   
			float tcorr[320];
			
		};
		struct ToTcal {
			int channel;
			float tot;
			float energy;
		};
	 	  
		float defaultQ;
		bool do_TQcorr;
		bool useEnergyCal;
		int tableSize;
		int TQtableSize;
		long totTableSize;
     
		
		TAC *table;
		TQ *TQtable;
		ToTcal *ToTtable;
	};
}}
#endif
