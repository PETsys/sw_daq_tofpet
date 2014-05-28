{
//  gStyle->SetOptFit(1);
/*  C001_14_0_T_control_T->SetLineColor(kBlack);
  C001_14_0_T_control_T->Draw();
  
  C001_14_1_T_control_T->SetLineColor(kRed);
  C001_14_1_T_control_T->Draw("SAME");

  C001_14_2_T_control_T->SetLineColor(kGreen);
  C001_14_2_T_control_T->Draw("SAME");

  C001_14_3_T_control_T->SetLineColor(kBlue);
  C001_14_3_T_control_T->Draw("SAME");*/
  
//  C001_14_0_T_control_E->Add(C001_14_1_T_control_E, 1);
  C001_14_0_T_control_E->Add(C001_14_2_T_control_E, 1);
//  C001_14_0_T_control_E->Add(C001_14_3_T_control_E, 1);
  C001_14_0_T_control_E->Draw();

}