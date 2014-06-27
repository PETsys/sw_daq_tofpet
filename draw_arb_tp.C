{
	FILE * txtFile = fopen("/tmp/tp_expo.txt", "r");
	long long txtTime;
	long long txtLength;	
	fscanf(txtFile, "%lld %lld\n", &txtTime, &txtLength);
	
	Long64_t treeTime;
	lmData->SetBranchAddress("time", &treeTime);
	lmData->GetEntry(0);
	int treeIndex = 0;	
	
	TH1F * hError = new TH1F("hError", "hError", 1024*256, 0, 6.4E6);
	
	Long64_t K = 3300000;
	
	while(true) {
		if(TMath::Abs(txtTime - (treeTime - K)) < 6.4E6) {
			printf("%lld - %lld = %lld\n", txtTime, (treeTime - K), txtTime - (treeTime -K));
			hError->Fill(txtTime - (treeTime - K));
			if(fscanf(txtFile, "%lld %lld\n", &txtTime, &txtLength) != 2)
				break;

			treeIndex += 1;
			if(treeIndex >= lmData->GetEntries())
				break;
			
			lmData->GetEntry(treeIndex);
		}
		else if(txtTime < treeTime) {
			if(fscanf(txtFile, "%lld %lld\n", &txtTime, &txtLength) != 2)
				break;
		}
		else {
			treeIndex += 1;
			if(treeIndex >= lmData->GetEntries())
				break;
			
			lmData->GetEntry(treeIndex);
		}
		
	}
	
	hError->Fit("gaus");
}