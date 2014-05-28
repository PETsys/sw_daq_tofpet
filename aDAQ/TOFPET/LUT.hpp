#ifndef __DAQ__TDC__LUT_HPP__DEFINED__
#define __DAQ__TDC__LUT_HPP__DEFINED__

namespace DAQ { namespace TOFPET {
	
	class LUT {
	public:
		LUT(int nChannels);
		~LUT();
		
		void setT0(int channel, int tac, bool isT, float t0);
		float getT0(int channel, int tac, bool isT);
		float getQ(int channel, int tac, bool isT, int adc);
		void setQ(int channel, int tac, bool isT, int adc, float q);
		float getT(int channel, int tac, bool isT, int adc, int coarse);
		bool isNormal(int channel, int tac, bool isT, int adc, int coarse);
		void loadFile(int start, int end, const char *fileName);
		void storeFile(int start, int end, const char *fileName);
		void loadFiles(const char *mapFileName);
		
		void setAll(float v);
	private:
		int  nChannels;
		int getIndex(int channel, int tac, bool isT);
		
		static const int nEntries = 1024;
		struct TAC {			
			float t0;
			float table[1024];
		};
		
		int tableSize;
		TAC *table;
		
	};
}}
#endif
