{
	gStyle->SetPalette(1);
	gStyle->SetOptFit(1);
	
	

	Float_t T = 6250;
	FILE * ctrTableA = fopen("ctr_mc_a.txt", "w");
	FILE * ctrTableB = fopen("ctr_mc_b.txt", "w");
	Float_t step1;      lmData->SetBranchAddress("step1", &step1);   
	Float_t step2;      lmData->SetBranchAddress("step2", &step2);   
	Float_t fTime1;		lmData->SetBranchAddress("time1", &fTime1);
	Float_t crystal1;	lmData->SetBranchAddress("crystal1", &crystal1);
	Float_t tot1;		lmData->SetBranchAddress("tot1", &tot1);
	Float_t fTime2;		lmData->SetBranchAddress("time2", &fTime2);
	Float_t crystal2;	lmData->SetBranchAddress("crystal2", &crystal2);
	Float_t tot2;		lmData->SetBranchAddress("tot2", &tot2);
	Float_t delta;		lmData->SetBranchAddress("delta", &delta);
	Float_t fTimediff;
	
	
	Int_t nCrystals = 128;
	TH1F *CI_hToT[nCrystals];
	Float_t CI_totPeak[nCrystals];
	Float_t CI_totSigma[nCrystals];
	TH1F *CI_hDelta[nCrystals];
	Float_t CI_deltaAdjust[nCrystals];
	bool CI_ignoreCrystals[nCrystals];
	
	
	TH1F *hDelta = new TH1F("hDelta", "Coincidence time difference", 2*10E-9/100E-12, -10E-9, 10E-9);
	for(Int_t crystal = 0; crystal < nCrystals; crystal++) {
		char hName[128];
		char hTitle[128];
		sprintf(hName, "hToT_%03d", crystal);
		sprintf(hTitle, "ToT of crystal %3d", crystal);
		CI_hToT[crystal] = new TH1F(hName, hTitle, 250, 0, 500);
		sprintf(hName, "hDelta_%03d", crystal);
		sprintf(hTitle, "Coincidence time difference for crystal %3d", crystal);
		CI_hDelta[crystal] = new TH1F(hName, hTitle, 2*10E-9/100E-12, -10E-9, 10E-9);	
	}
	TF1 *pdf1 = new TF1("gaus_base", "[0]*exp(-0.5*((x-[1])/[2])**2) + [3]", -10E-9, 10E-9);
	pdf1->SetNpx(CI_hDelta[0]->GetNbinsX());
	pdf1->SetParName(0, "Constant");
	pdf1->SetParName(1, "Mean");
	pdf1->SetParName(2, "Sigma");
	pdf1->SetParName(3, "Base");	

	Int_t stepBegin = 0;
	Int_t nEvents = lmData->GetEntries();

	do {	
		for(Int_t crystal = 0; crystal < nCrystals; crystal++) {
			CI_hToT[crystal]->Reset();
			CI_hDelta[crystal]->Reset();
			CI_totPeak[crystal] = 0;
			CI_totSigma[crystal] = 0;
			CI_deltaAdjust[crystal] = 0;
			CI_ignoreCrystals[crystal] = false;
		}
	
		lmData->GetEntry(stepBegin);
		Float_t currentStep1 = step1;
		Float_t currentStep2 = step2;		
		Int_t stepEnd = stepBegin + 1;
		while(stepEnd < nEvents) {
			lmData->GetEntry(stepEnd);
			Int_t c1 = (Int_t(crystal1));
			Int_t c2 = (Int_t(crystal2));
			
			if(step1 != currentStep1 || step2 != currentStep2) 
				break;

			CI_hToT[c1]->Fill(tot1);
			CI_hToT[c2]->Fill(tot2);
			stepEnd++;
		}		
		
		printf("Step (%6.3f %6.3f) event range: %d to %d\n", currentStep1, currentStep2, stepBegin, stepEnd);	
		

		TSpectrum *spectrum = new TSpectrum();
		for(Int_t crystal = 0; crystal < nCrystals; crystal++) {
			if(CI_hToT[crystal]->GetEntries() == 0)
				continue;
			
			spectrum->Search(CI_hToT[crystal], 3, " ",  0.2);
			Int_t nPeaks = spectrum->GetNPeaks();
			if (nPeaks == 0) {
//				printf("No peaks in %s!!!\n", CI_hToT[crystal]->GetName());
				continue;
			}		
			
			Float_t *xPositions = spectrum->GetPositionX();
			Float_t x_psc = 0;
			Float_t x_pe = 0;
			for(Int_t i = 0; i < nPeaks; i++) {
				if(xPositions[i] > x_pe) {
					if(x_pe > x_psc)
						x_psc = x_pe;
					x_pe = xPositions[i];
				}
			} 			 
			
			Float_t x = x_pe;
			CI_hToT[crystal]->Fit("gaus", "", "", x-10, x+10);
			if(CI_hToT[crystal]->GetFunction("gaus") == NULL) { 
				continue; 
			}
			
			x = CI_hToT[crystal]->GetFunction("gaus")->GetParameter(1);
			Float_t sigma = CI_hToT[crystal]->GetFunction("gaus")->GetParameter(2);
			
			if(x < 50E)
				sigma = -1;
			
			CI_totPeak[crystal] = x;
			CI_totSigma[crystal] = sigma;
			
			
			
		}
		
	
		Int_t nIterations = 20;
		Float_t sN = 2.0;
		for(Int_t iteration = 0; iteration < nIterations; iteration ++) {
			printf("Iteration %d\n", iteration);
			hDelta->Reset();
			for(Int_t crystal = 0; crystal < nCrystals; crystal++) {
				CI_hDelta[crystal]->Reset();
			}
			
			for(Int_t i = stepBegin; i < stepEnd; i++) {
				lmData->GetEntry(i);
				Int_t c1 = (Int_t(crystal1));
				Int_t c2 = (Int_t(crystal2));

				
				if(fabs(tot1 - CI_totPeak[c1]) > sN*CI_totSigma[c1])
					continue;
				
				if(fabs(tot2 - CI_totPeak[c2]) > sN*CI_totSigma[c2])
					continue;
				
				if(CI_ignoreCrystals[c1]) continue;
				if(CI_ignoreCrystals[c2]) continue;
				
				Float_t correctDelta = delta*1E-12 + CI_deltaAdjust[c1] - CI_deltaAdjust[c2];
				CI_hDelta[c1]->Fill(+correctDelta);
				CI_hDelta[c2]->Fill(-correctDelta);
				hDelta->Fill(correctDelta);
				
			}
			
			for(Int_t crystal = 0; crystal < nCrystals; crystal++) {				
				if(CI_hDelta[crystal]->GetEntries() == 0) continue;
				
			
				float hdMax = CI_hDelta[crystal]->GetMaximum();
				float hdMean = CI_hDelta[crystal]->GetBinCenter(CI_hDelta[crystal]->GetMaximumBin());
				float hdRMS = CI_hDelta[crystal]->GetRMS();
				float hdAvg = CI_hDelta[crystal]->GetEntries()/CI_hDelta[crystal]->GetNbinsX();
				pdf1->SetParameter(0, hdMax);	pdf1->SetParLimits(0, hdAvg, 2*hdMax);
				pdf1->SetParameter(1, hdMean);	pdf1->SetParLimits(1, hdMean - 0.5*hdRMS, hdMean + 0.5*hdRMS);
				pdf1->SetParameter(2, hdRMS);	pdf1->SetParLimits(2, 100E-12, 1.5 * hdRMS);
				pdf1->SetParameter(3, 0);	pdf1->SetParLimits(3, 0, 3*hdAvg);				
				CI_hDelta[crystal]->Fit("gaus_base", "Q");
				
				TF1 *fit = CI_hDelta[crystal]->GetFunction("gaus_base");			
				
				if(fit == NULL) continue;
				Float_t chi2 = fit->GetNDF() !=  0 ? fit->GetChisquare()/fit->GetNDF() : 1E9;
				Float_t mean = (chi2 < 5.0) ? fit->GetParameter(1) : CI_hDelta[crystal]->GetMean();
				Float_t sigma = (chi2 < 5.0) ? fit->GetParameter(2) : CI_hDelta[crystal]->GetRMS();
				
				Float_t iterationWeight = (iteration < 0.1 * nIterations) ? 1.0 :
							  (iteration == nIterations - 1) ? 0.0 :
							  0.5;
							  
				Float_t newAdjust = CI_deltaAdjust[crystal] - iterationWeight * mean;
				
				if(true) {
					printf("Crystal %2d: rms: %8.3e, mean: %8.3e, adjust %8.3e => %8.3e\n", crystal, sigma, mean, CI_deltaAdjust[crystal], newAdjust);
/*					char hName[128];
					sprintf(hName, "%s_%d", CI_hDelta[crystal]->GetName(), iteration);
					CI_hDelta[crystal]->Clone(hName);*/
					
				}
				
				CI_deltaAdjust[crystal] = newAdjust;
/*				if(fabs(mean) > iterationDeltaBiasLimit[iteration])
					CI_ignoreCrystals[crystal] = true;*/
								
			}
			
			float hdMax = hDelta->GetMaximum();
			float hdMean = hDelta->GetBinCenter(hDelta->GetMaximumBin());
			float hdRMS = hDelta->GetRMS();
			float hdAvg = hDelta->GetEntries()/hDelta->GetNbinsX();
			pdf1->SetParameter(0, hdMax);	pdf1->SetParLimits(0, hdAvg, 2*hdMax);
			pdf1->SetParameter(1, hdMean);	pdf1->SetParLimits(1, hdMean - 0.5*hdRMS, hdMean + 0.5*hdRMS);
			pdf1->SetParameter(2, hdRMS);	pdf1->SetParLimits(2, 100E-12, 1.5 * hdRMS);
			pdf1->SetParameter(3, 0);	pdf1->SetParLimits(3, 0, 3*hdAvg);				
			hDelta->Fit("gaus_base");
			
			
			
		}
		
		for(Int_t crystal = 0; crystal < nCrystals; crystal++) {
			if(CI_hToT[crystal]->GetEntries() == 0 || CI_hDelta[crystal]->GetEntries() == 0) { 
				continue;
			}
			
			TF1 *totFit = CI_hToT[crystal]->GetFunction("gaus");
			TF1 *ctrFit = CI_hDelta[crystal]->GetFunction("gaus_base");
			if (totFit == NULL || ctrFit == NULL) continue;
			
			fprintf(ctrTableA, 
				"%8.3f %8.3f %3d %12.3e %12.3e %12.3e %12.3e\n",
				step1, step2,
				crystal,
				totFit->GetParameter(1), totFit->GetParError(1),
				ctrFit->GetParameter(2), ctrFit->GetParError(2)
				);
		}		
		fflush(ctrTableA);
		
		
		TF1 *ctrFit = hDelta->GetFunction("gaus_base");
		fprintf(ctrTableB, "%8.3f %8.3f %12.3e %12.3e\n", 
			step1, step2,
			ctrFit->GetParameter(2), ctrFit->GetParError(2)
			);
		fflush(ctrTableB);
			

		//break;
		stepBegin = stepEnd;			
	} while(stepBegin < nEvents); 
	
	fclose(ctrTableA);
	fclose(ctrTableB);
	for(Int_t crystal = 0; crystal < nCrystals; crystal++) {
		if(CI_hToT[crystal]->GetEntries() == 0 || CI_hDelta[crystal]->GetEntries() == 0) { 
			delete CI_hToT[crystal];
			delete CI_hDelta[crystal];
		}
		
	}	

}
