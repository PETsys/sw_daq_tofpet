{
	Float_t T = 6250;
	char strTableName[256]; strTableName[0]=0;
	FILE * strTable[64][64];
	TFile *rootFile = new TFile("str.root", "RECREATE");
	
	TNtuple *data = (TNtuple *)_file0->Get("data");
	Float_t step1;      	data->SetBranchAddress("step1", &step1);   
	Float_t step2;      	data->SetBranchAddress("step2", &step2);   
	Float_t channel;		data->SetBranchAddress("channel", &channel);
	Float_t tac;		data->SetBranchAddress("tac", &tac);
	Float_t tcoarse; 		data->SetBranchAddress("tcoarse", &tcoarse);
	Float_t tfine; 		data->SetBranchAddress("tfine", &tfine);
	Float_t ecoarse; 		data->SetBranchAddress("ecoarse", &ecoarse);
	Float_t efine; 		data->SetBranchAddress("efine", &efine);
	


	gStyle->SetPalette(1);
	gStyle->SetOptFit(1);

    
	TH3F * hToT3D = new TH3F("hToT2D", "ToT", 1024, 0, 1024*T/1000, 64,0,64, 64,0,64);


	TH3F * hTFine3D = new TH3F("hTFine2D", "TFine", 1024, 0, 1024, 64,0,64, 64,0,64);

	
	TH3F * hEFine3D = new TH3F("hEFine2D", "EFine", 1024, 0, 1024, 64,0,64, 64,0,64);



	
	Int_t stepBegin = 0;
	Int_t nEvents = data->GetEntries();

	while (stepBegin < nEvents) {
		hToT2D->Reset();
		hTFine2D->Reset();
		hEFine2D->Reset();

		data->GetEntry(stepBegin);
		
		Float_t currentStep1 = step1;
		Float_t currentStep2 = step2;
		printf("Step (%6.3f %6.3f)\n", currentStep1, currentStep2);
		
		Int_t stepEnd = stepBegin + 1;
		while(stepEnd < nEvents) {
			data->GetEntry(stepEnd);	    
			if(step1 != currentStep1 || step2 != currentStep2) 
				break;			
			stepEnd++;	
			
			//if((Int_t(channel) / 64) != ASIC) continue;
			if(Int_t(tac) != 0) continue;
			
			Int_t tot = ecoarse - tcoarse;

		
			Int_t ch= Int_t(channel);


			hToT3D->Fill(tot * T / 1000,ch/64, ch%64);
			hTFine3D->Fill(tfine,ch/64, ch%64);
			hEFine3D->Fill(efine,ch/64, ch%64);
	
		}

		
		Int_t thisStepBegin = stepBegin;
		Int_t thisStepEnd = stepEnd;
		stepBegin = stepEnd;
		
		printf("Event range: %d to %d\n", stepBegin, stepEnd);
		
		for(Int_t i = 1; i<hToT3D->GetNbinsZ()+1;i++){
			//if(stop_flag==true)break;
			
				
			Int_t ch=i-1;
				
			TH1 *hToT = hToT3D->ProjectionX("hToT", 1, 64,i,i);
	
				
			if(hToT->GetEntries() > 100){
				for(Int_t j=1; j< hToT3D->GetNbinsY()+1;j++){
					hToT->Reset();
			
					Int_t asic=j-1;
					TH1 *hToT = hToT3D->ProjectionX("hToT", j, j,i,i);
					TH1 *hTFine = hTFine3D->ProjectionX("hTFine", j, j,i,i);
					TH1 *hEFine = hEFine3D->ProjectionX("hEFine", j, j,i,i);
					hToT->GetXaxis()->SetTitle("ToT (ns)");
					hTFine->GetXaxis()->SetTitle("TFine (LSB)");			
					hEFine->GetXaxis()->SetTitle("EFine (LSB)");
				
					Int_t asic=j-1;
					if(hToT->GetEntries() >100){	
						if(strTable[asic][ch] == NULL){
							
							sprintf(strTableName,"str_%d_%d.txt",asic,ch);
					
							strTable[asic][ch] = fopen(strTableName, "w");	  
						}
					
						hToT->Fit("gaus");
						hTFine->Fit("gaus");
						hEFine->Fit("gaus");
						
			
						char hName[1024];
						sprintf(hName, "hToT_%05d_%05d_%03d_%05d", int(step1*1000), int(step2*1000), asic,ch);
						hToT->Clone(hName);
						sprintf(hName, "hTFine_%05d_%05d_%03d_%05d", int(step1*1000), int(step2*1000),asic,ch);
						hTFine->Clone(hName);
						sprintf(hName, "hEFine_%05d_%05d_%03d_%05d", int(step1*1000), int(step2*1000),asic,ch);
						hEFine->Clone(hName);
						
						
						TF1 *fToT = hToT->GetFunction("gaus");
						TF1 *fTFine = hTFine->GetFunction("gaus");
						TF1 *fEFine = hEFine->GetFunction("gaus");
						
						if(fToT != NULL && fTFine != NULL && fEFine != NULL) {    
							/*
							printf("%f\t%f\t%f\t%f\t%e\t%e\t%e\t%e\t%d\t%d\n", 
								   step1, step2, 
								   fToT->GetParameter(1), fEFine->GetParameter(2),
								   fabs(fTFine->GetParameter(2)), fTFine->GetParError(2),
								   fTFine->GetParameter(1), fTFine->GetParError(1)
								   ,asic,ch);*/
							fprintf(strTable[asic][ch], "%f\t%f\t%f\t%f\t%e\t%e\t%e\t%e\t\n", 
									step1, step2, 
									fToT->GetParameter(1), fEFine->GetParameter(2),
									fabs(fTFine->GetParameter(2)), fTFine->GetParError(2),
									fTFine->GetParameter(1), fTFine->GetParError(1)
									);
							
							fflush(strTable[asic][ch]);
							
						}
		   
					}	
				}		
			}
			
		}
	}				   	   			
	for(Int_t i=0;i<64;i++){
		for(Int_t j=0;j<64;j++){
			if(strTable[i][j] != NULL){
				fclose(strTable[i][j]);	
			}
		}
	}
	
	rootFile->Write();
}
