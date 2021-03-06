#include "TFile.h"
#include "UniqueEntryListMaker.h"
#include "TError.h"
#include "TCanvas.h"

UniqueEntryListMaker::UniqueEntryListMaker(TTree *)
{
  EntryList = 0;
  hlog = 0;
}

void UniqueEntryListMaker::Init(TTree *tree)
{
  //Called every time a new TTree is attached.
  fReader.SetTree(tree);
}

Bool_t UniqueEntryListMaker::Notify() {
  std::clog << Form("Processing: %s\n",(fReader.GetTree())->GetCurrentFile()->GetEndpointUrl()->GetUrl());
  return true;
}

void UniqueEntryListMaker::Begin(TTree *tree) {

  if (fInput->FindObject("SampleName")) {
    // Lesson: TString can't be in TCollection
    TNamed *p = dynamic_cast<TNamed *>(fInput->FindObject("SampleName"));
    SampleName = p->GetTitle();
  }
  if (fInput){
    EntryList = new TEntryList("EntryList","Entry Number");
    fInput->Add(EntryList);
  }

  if (fInput->FindObject("Year")) {
    TParameter<Int_t> *p = dynamic_cast<TParameter<Int_t>*>(fInput->FindObject("Year"));
    Year = p->GetVal();
  }

  std::clog << "SampleName: " << SampleName << std::endl;
  std::clog << "EntryList: " << EntryList << std::endl;
}

void UniqueEntryListMaker::SlaveBegin(TTree *tree) {

  hlog = new TH1D("hlog","hlog",100,0.,100.);
  fOutput->Add(hlog);

  if (fInput->FindObject("SampleName")) {
    TNamed *p = dynamic_cast<TNamed *>(fInput->FindObject("SampleName"));
    SampleName = p->GetTitle();
  }

  if((EntryList = (TEntryList*) fInput->FindObject("EntryList")))
    EntryList = (TEntryList *) EntryList->Clone();

  if(EntryList)
    fOutput->Add(EntryList);

  if (fInput->FindObject("EventIndexTree1"))
    AddTreeToEventIndex("EventIndexTree1");
  if (fInput->FindObject("EventIndexTree2"))
    AddTreeToEventIndex("EventIndexTree2");

}

void UniqueEntryListMaker::AddTreeToEventIndex(std::string_view treeName){
  EventIndexTree = dynamic_cast<TTree *>(fInput->FindObject(treeName.data()));
  TTreeReader fReader1(EventIndexTree);
  TTreeReaderValue<Long64_t> EvID(fReader1,"EventID");

  while(fReader1.Next()){
    hlog->Fill(treeName.data(),1.);
    if (!(EventIndex.insert(*EvID).second))
      std::clog << Form("\tDuplicated EvID: %s %lld\n",treeName.data(),*EvID) ;
  }
}
Long64_t UniqueEntryListMaker::GetEventIndex(const UInt_t& run,const ULong64_t& event) {
  Long64_t uid = std::stoll(std::to_string(run)+std::to_string(event));
  assert(uid<numeric_limits<Long64_t>::max());
  return uid;
}

Bool_t UniqueEntryListMaker::Process(Long64_t entry) {

   fReader.SetEntry(entry);

   EventID = GetEventIndex(*run,*event);
   if(EventIndex.find(EventID) == EventIndex.end ()) {
     hlog->Fill(Form("UniqueEvent_%s",SampleName.Data()),1.);
     EntryList->Enter(entry);
   } else {
     hlog->Fill(Form("DuplicatedEvent_%s",SampleName.Data()),1.);
   }

   return kTRUE;
}

void UniqueEntryListMaker::Terminate() {

  std::unique_ptr<TCanvas> ch(new TCanvas("ch","ch",1200,800));
  gPad->SetLogy();
  hlog->LabelsDeflate();
  hlog->Draw("HIST TEXT45");
  ch->Print(Form("UniqueEntryListMaker_%s_%d_hlog.png",SampleName.Data(),Year));

  EntryList = dynamic_cast<TEntryList*>(fOutput->FindObject("EntryList"));
  std::unique_ptr<TFile> fEntryList(TFile::Open("EntryLists_Unique.root","UPDATE"));
  fEntryList->mkdir(Form("%s_%d",SampleName.Data(),Year));
  fEntryList->cd(Form("%s_%d",SampleName.Data(),Year));
  hlog->SetName(Form("hlog_%s_%d",SampleName.Data(),Year));
  hlog->Write();
  EntryList->Write();
  fEntryList->Close();

}
