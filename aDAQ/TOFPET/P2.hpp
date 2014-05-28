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

		float getQ(int channel, int tac, bool isT, int adc, long long tacIdleTime);
		bool isNormal(int channel, int tac, bool isT, int adc, int coarse, long long tacIdleTime);
		float getT(int channel, int tac, bool isT, int adc, int coarse, long long tacIdleTime);

		void loadFile(int start, int end, const char *fileName);
		void storeFile(int start, int end, const char *fileName);
		void loadFiles(const char *mapFileName);
		
		void setAll(float v);
	private:
		int  nChannels;
		int getIndex(int channel, int tac, bool isT);
		
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
		float defaultQ;
		
		int tableSize;
		TAC *table;
		
	};
}}
#endif
