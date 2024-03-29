#pragma GCC diagnostic ignored "-Wsign-compare"
#include "TFile.h"
#include "TH1D.h"
#include "TTree.h"
#include "TChain.h"
#include "TTreeCache.h"
#include "TTreeCacheUnzip.h"
#include "TTreePerfStats.h"
#include "TLorentzVector.h"
#include "math.h"
#include "TVector2.h"
#include "TVector3.h"
#include "TRandom3.h"

#include "RooRealVar.h"
#include "RooDataSet.h"
#include "RooDataHist.h"
#include "RooCategory.h"

#include "../NanoCORE/Nano.h"
#include "../NanoCORE/Base.h"
#include "../NanoCORE/tqdm.h"
#include "../NanoCORE/ZPrimeTools.cc"
#include "../NanoCORE/XYMETCorrection_withUL17andUL18andUL16.h"
#include "../NanoCORE/Tools/goodrun.h"
#include "../NanoCORE/Tools/dorky.h"
#include "../NanoCORE/Tools/puWeight.h"
#include "../NanoCORE/Tools/muonRecoSF.h"
#include "../NanoCORE/Tools/muonIDSF.h"
#include "../NanoCORE/Tools/muonIsoSF.h"
#include "../NanoCORE/Tools/muonTriggerSF.h"
#include "../NanoCORE/Tools/bTagEff.h"
#include "../NanoCORE/Tools/btagsf/BTagCalibrationStandalone_v2.h"
#include "../NanoCORE/Tools/jetcorr/JetCorrectionUncertainty.h"
#include "../NanoCORE/DiPhotonSelections.h"
#include "../NanoCORE/LeptonSelections.h"
#include "../NanoCORE/DiJetSelections.h"
#include "../NanoCORE/GenPart.h"

#include "configuration_Zp.h"

#include <iostream>
#include <iomanip>
#include <sys/stat.h>
#include <fstream>

#define SUM(vec) std::accumulate((vec).begin(), (vec).end(), 0);
#define SUM_GT(vec,num) std::accumulate((vec).begin(), (vec).end(), 0, [](float x,float y) { return ((y > (num)) ? x+y : x); });
#define COUNT_GT(vec,num) std::count_if((vec).begin(), (vec).end(), [](float x) { return x > (num); });
#define COUNT_LT(vec,num) std::count_if((vec).begin(), (vec).end(), [](float x) { return x < (num); });

#define H1(name,nbins,low,high,xtitle) TH1D *h_##name = new TH1D(#name,"",nbins,low,high); h_##name->GetXaxis()->SetTitle(xtitle); h_##name->GetYaxis()->SetTitle("Events");

// #define DEBUG
#define Zmass 91.1876

// For testing purposes only
bool useOnlyRun2018B = true;

// Looper setup flags
bool muonDebug = false;
bool doMllBins = false;
bool doNbTagBins = true;
bool doTTEnriched = false;
bool doDYEnriched = false;
bool doMuDetRegionBins = false;

// General flags
bool removeSpikes = true;
bool removeDataDuplicates = false;
bool useTuneP = true;
bool usePuppiMET = true;
bool fillRooDataSet = true;
//

const char* outdir = "temp_data";
int mdir = mkdir(outdir,0755);

struct debugger { template<typename T> debugger& operator , (const T& v) { cerr<<v<<" "; return *this; } } dbg;
#ifdef DEBUG
#define debug(args...) do {cerr << #args << ": "; dbg,args; cerr << endl;} while(0)
#else
#define debug(args...)
#endif

using namespace std;
using namespace tas;
using namespace duplicate_removal;
using namespace RooFit;
int count_test=0;

ofstream txtout("evtnb.txt", ofstream::app);

int ScanChain_Hgg(TChain *ch, double genEventSumw, TString year, TString process, int topPtWeight=1, int PUWeight=1, int muonSF=1, int triggerSF=1, int bTagSF=1, int JECUnc=0) {
// Event weights / scale factors:
//  0: Do not apply
//  1: Apply central value
// +2: Apply positive variation
// -2: Apply negative variation

  float factor = 1.0;
  float lumi = 1.0;
  float xsec = 1.0;
  bool isMC = true;
  int process_id = 0;

  cout << "Process " << process << endl;

  if ( process.Contains("EGamma_Run2018") ) {
    isMC = false;
  }
  // SM processes and cross-sections:
  // set this in a different file
  else if ( process == "ttbar" )             {xsec = 87310.0; /* fb */}
  else if ( process == "DY" )                {xsec = 5765400.0; /* fb */ process_id = 16; }
  else if ( process == "WW" )                xsec = 118700.0; // fb 
  else if ( process == "WZ" )                xsec = 47130.0; // fb
  else if ( process == "ZZ" )                xsec = 16523.0; // fb
  else if ( process == "tW" )                xsec = 19550; // fb
  else if ( process == "tbarW" )             xsec = 19550; // fb
  else if ( process == "tZq" )               xsec = 75.8; // fb
  else if ( process == "TTW" )               xsec = 204.3; // fb
  else if ( process == "TTZ" )               xsec = 252.9; // fb
  else if ( process == "TTHToNonbb" )        xsec = 507.5*(1-0.575); // fb
  else if ( process == "TTHTobb" )           xsec = 507.5*0.575; // fb
  else if ( process == "TTGG" )              {xsec = 0.01687 * 1000; /*fb*/ process_id = 12;} 
  else if ( process == "TTGJets" )           {xsec = 4.078 * 1000; /*fb*/ process_id = 11;} 
  else if ( process == "TTJets" )            {xsec = 831.76 * 1000; /*fb*/ process_id = 10;}
  else if ( process == "VBFH_M125" )         {xsec = 0.00858514 *1000; /*fb*/ process_id = 3;}
  else if ( process == "VH_M125" )           {xsec = 0.00512 *1000; /*fb*/ process_id = 4;}
  else if ( process == "ggHToDiPhoM125" )    {xsec = 0.1118429*1000 ; /* fb */ process_id = 2;}
  else if ( process == "ttH_M125" )          {xsec = 0.5071 * 1000 * 0.00227; /*fb*/ process_id = 1;}
  else if ( process == "GJets_HT-40To100" )  {xsec = 23100*1000; /*fb*/ process_id = 5;}
  else if ( process == "GJets_HT-100To200" ) {xsec = 8631.0*1000; /*fb*/ process_id = 6;}
  else if ( process == "GJets_HT-200To400" ) {xsec = 2280.0*1000; /*fb*/ process_id = 7;}
  else if ( process == "GJets_HT-400To600" ) {xsec = 273*1000; /*fb*/ process_id = 8;}
  else if ( process == "GJets_HT-600ToInf" ) {xsec = 1*1000; /*fb*/ process_id = 9;}
  else if ( process == "diPhoton" )          {xsec = 84.4*1000 ; /* fb */ process_id = 15; }
  else if ( process == "HHbbgg" )            {xsec = 0.03105*1000*0.0026223039999999998 ; /*fb*/ process_id = 17;}
  else if ( process == "WGamma" )            {xsec = 191.4*1000 ; /* fb */ process_id = 13;}
  else if ( process == "ZGamma" )            {xsec = 55.6*1000 ; /* fb */ process_id = 14;}
  else if ( process.Contains("EGamma_Run2018") )   {xsec = 1 ; /*fb*/ process_id = 0;}
  else if ( process == "NMSSM_XYH_Y_gg_H_bb_MX_600_MY_100" ) {xsec = 1 ; /* fb */ process_id = 20;}
  else if ( process == "NMSSM_XYH_Y_gg_H_bb_MX_600_MY_90" ) {xsec = 1 ; /* fb */ process_id = 18;}
  else if ( process == "NMSSM_XYH_Y_gg_H_bb_MX_600_MY_95" ) {xsec = 1 ; /* fb */ process_id = 19;}
  else if ( process == "NMSSM_XYH_Y_gg_H_bb_MX_650_MY_100" ) {xsec = 1 ; /* fb */ process_id = 23;}
  else if ( process == "NMSSM_XYH_Y_gg_H_bb_MX_650_MY_90" ) {xsec = 1 ; /* fb */ process_id = 21;}
  else if ( process == "NMSSM_XYH_Y_gg_H_bb_MX_650_MY_95" ) {xsec = 1 ; /* fb */ process_id = 22;}
  else if ( process == "NMSSM_XYH_Y_gg_H_bb_MX_700_MY_100" ) {xsec = 1 ; /* fb */ process_id = 26;}
  else if ( process == "NMSSM_XYH_Y_gg_H_bb_MX_700_MY_90" ) {xsec = 1 ; /* fb */ process_id = 24;}
  else if ( process == "NMSSM_XYH_Y_gg_H_bb_MX_700_MY_95" ) {xsec = 1 ; /* fb */ process_id = 25;}
    
//  111.8429 ; // fb
  // Signal processes and cross-sections:
  else
    {
      cout<<"Non-valid process: Exiting!"<<endl;
      return 1;
    }

  // Configuration setup: NanoCORE/Config.{h,cc}
  gconf.nanoAOD_ver = 9;
  gconf.GetConfigs(year.Atoi());
  lumi = gconf.lumi;
  if (year == "2018") lumi=59.8; //synchronizing with HiggsDNA
//  lumi=1;

  // Golden JSON files
  if ( !isMC ) {
    if ( year == "2016APV" )
      set_goodrun_file_json("../utils/Cert_271036-284044_13TeV_Legacy2016_Collisions16_JSON.txt");
    if ( year == "2016nonAPV" )
      set_goodrun_file_json("../utils/Cert_271036-284044_13TeV_Legacy2016_Collisions16_JSON.txt");
    if ( year == "2017" )
      set_goodrun_file_json("../utils/Cert_294927-306462_13TeV_UL2017_Collisions17_GoldenJSON.txt");
    if ( year == "2018" )
      set_goodrun_file_json("../utils/Cert_314472-325175_13TeV_Legacy2018_Collisions18_JSON.txt");
  }

  if ( isMC )
    factor = xsec*lumi/genEventSumw;

  // Modify the name of the output file to include arguments of ScanChain function (i.e. process, year, etc.)
  TFile* fout = new TFile("temp_data/output_"+process+"_"+year+".root", "RECREATE");
  TTree* tout = new TTree("tout","Tree with photon variables");

  // define histograms, to be put in a different file TODO
  H1(LeadPhoton_sieie, 20, 0, 0.05, "");
  H1(LeadPhoton_pfPhoIso03, 20, 0, 10, "");
  H1(LeadPhoton_chargedHadronIso, 20, 0, 10, "");
  H1(LeadPhoton_trkSumPtHollowConeDR03, 20, 0, 10, "");
  H1(SubleadPhoton_sieie, 20, 0, 0.05, "");
  H1(SubleadPhoton_pfPhoIso03, 20, 0, 10, "");
  H1(SubleadPhoton_chargedHadronIso, 20, 0, 10, "");
  H1(SubleadPhoton_trkSumPtHollowConeDR03, 20, 0, 10, "");

  Float_t xcand_pt, xcand_eta, xcand_phi, xcand_mass;

  Float_t LeadPhoton_pt, LeadPhoton_eta, LeadPhoton_phi, LeadPhoton_mass, LeadPhoton_mvaID;
  Bool_t LeadPhoton_pixelSeed, SubleadPhoton_pixelSeed;
  Int_t LeadPhoton_genPartFlav, SubleadPhoton_genPartFlav;
  Float_t SubleadPhoton_pt, SubleadPhoton_eta, SubleadPhoton_phi, SubleadPhoton_mass, SubleadPhoton_mvaID;
  Float_t Diphoton_pt, Diphoton_eta, Diphoton_phi, Diphoton_mass, Diphoton_pt_mgg, Diphoton_dR;
  Float_t LeadPhoton_sieie, LeadPhoton_pfPhoIso03, LeadPhoton_trkSumPtHollowConeDR03, LeadPhoton_chargedHadronIso, LeadPhoton_r9;
  Float_t SubleadPhoton_sieie, SubleadPhoton_pfPhoIso03, SubleadPhoton_trkSumPtHollowConeDR03, SubleadPhoton_chargedHadronIso, SubleadPhoton_r9;

  Int_t n_jets;
  Float_t dijet_lead_pt, dijet_lead_eta, dijet_lead_phi, dijet_lead_mass, dijet_lead_btagDeepFlavB;
  Float_t dijet_sublead_pt, dijet_sublead_eta, dijet_sublead_phi, dijet_sublead_mass, dijet_sublead_btagDeepFlavB;
  Float_t dijet_pt, dijet_eta, dijet_phi, dijet_mass, dijet_dR;
  Int_t year_out, eventNum, weight_central, weight_central_initial, weight_central_no_lumi;

  int n_gen_matched_jets = 0, n_gen_matched_in_dijet = 0;
  bool dijet_lead_gen_match=false, dijet_sublead_gen_match=false;
  float GenHiggs_pt=-999, GenHiggs_eta=-999, GenHiggs_phi=-999, GenHiggs_mass=-999, GenHiggs_dR=-999;
  float GenY_pt=-999, GenY_eta=-999, GenY_phi=-999, GenY_mass=-999, GenY_dR=-999;
  float GenX_pt=-999, GenX_eta=-999, GenX_phi=-999, GenX_mass=-999, GenX_dR=-999;
  float GenBFromHiggs_1_pt=-999, GenBFromHiggs_1_eta=-999, GenBFromHiggs_1_phi=-999, GenBFromHiggs_1_mass=-999;
  float GenBFromHiggs_2_pt=-999, GenBFromHiggs_2_eta=-999, GenBFromHiggs_2_phi=-999, GenBFromHiggs_2_mass=-999;

  tout->Branch("xcand_pt", &xcand_pt, "xcand_pt/F");
  tout->Branch("xcand_eta", &xcand_eta, "xcand_eta/F");
  tout->Branch("xcand_phi", &xcand_phi, "xcand_phi/F");
  tout->Branch("xcand_mass", &xcand_mass, "xcand_mass/F");

  tout->Branch("LeadPhoton_pt",&LeadPhoton_pt,"LeadPhoton_pt/F");
  tout->Branch("LeadPhoton_eta",&LeadPhoton_eta,"LeadPhoton_eta/F");
  tout->Branch("LeadPhoton_phi",&LeadPhoton_phi,"LeadPhoton_phi/F");
  tout->Branch("LeadPhoton_mass",&LeadPhoton_mass,"LeadPhoton_mass/F");
  tout->Branch("LeadPhoton_pixelSeed",&LeadPhoton_pixelSeed,"LeadPhoton_pixelSeed/B");
  tout->Branch("LeadPhoton_r9",&LeadPhoton_r9,"LeadPhoton_r9/F");
  tout->Branch("LeadPhoton_sieie",&LeadPhoton_sieie,"LeadPhoton_sieie/F");
  tout->Branch("LeadPhoton_pfPhoIso03",&LeadPhoton_pfPhoIso03,"LeadPhoton_pfPhoIso03/F");
  tout->Branch("LeadPhoton_trkSumPtHollowConeDR03",&LeadPhoton_trkSumPtHollowConeDR03,"LeadPhoton_trkSumPtHollowConeDR03/F");
  tout->Branch("LeadPhoton_chargedHadronIso",&LeadPhoton_chargedHadronIso,"LeadPhoton_chargedHadronIso/F");
  tout->Branch("LeadPhoton_mvaID",&LeadPhoton_mvaID,"LeadPhoton_mvaID/F");

  tout->Branch("SubleadPhoton_pt",&SubleadPhoton_pt,"SubleadPhoton_pt/F");
  tout->Branch("SubleadPhoton_eta",&SubleadPhoton_eta,"SubleadPhoton_eta/F");
  tout->Branch("SubleadPhoton_phi",&SubleadPhoton_phi,"SubleadPhoton_phi/F");
  tout->Branch("SubleadPhoton_mass",&SubleadPhoton_mass,"SubleadPhoton_mass/F");
  tout->Branch("SubleadPhoton_pixelSeed",&SubleadPhoton_pixelSeed,"SubleadPhoton_pixelSeed/B");  
  tout->Branch("SubleadPhoton_r9",&SubleadPhoton_r9,"SubleadPhoton_r9/F");
  tout->Branch("SubleadPhoton_sieie",&SubleadPhoton_sieie,"SubleadPhoton_sieie/F");
  tout->Branch("SubleadPhoton_pfPhoIso03",&SubleadPhoton_pfPhoIso03,"SubleadPhoton_pfPhoIso03/F");
  tout->Branch("SubleadPhoton_trkSumPtHollowConeDR03",&SubleadPhoton_trkSumPtHollowConeDR03,"SubleadPhoton_trkSumPtHollowConeDR03/F");
  tout->Branch("SubleadPhoton_chargedHadronIso",&SubleadPhoton_chargedHadronIso,"SubleadPhoton_chargedHadronIso/F");
  tout->Branch("SubleadPhoton_mvaID",&SubleadPhoton_mvaID,"SubleadPhoton_mvaID/F");

  tout->Branch("Diphoton_pt",&Diphoton_pt,"Diphoton_pt/F");
  tout->Branch("Diphoton_eta",&Diphoton_eta,"Diphoton_eta/F");
  tout->Branch("Diphoton_phi",&Diphoton_phi,"Diphoton_phi/F");
  tout->Branch("Diphoton_mass",&Diphoton_mass,"Diphoton_mass/F");
  tout->Branch("Diphoton_pt_mgg",&Diphoton_pt_mgg,"Diphoton_pt_mgg/F");
  tout->Branch("Diphoton_dR",&Diphoton_dR,"Diphoton_dR/F");

  tout->Branch("n_jets",&n_jets,"n_jets/I");  
  tout->Branch("dijet_lead_pt",&dijet_lead_pt,"dijet_lead_pt/F");
  tout->Branch("dijet_lead_eta",&dijet_lead_eta,"dijet_lead_eta/F");
  tout->Branch("dijet_lead_phi",&dijet_lead_phi,"dijet_lead_phi/F");
  tout->Branch("dijet_lead_mass",&dijet_lead_mass,"dijet_lead_mass/F");
  tout->Branch("dijet_lead_btagDeepFlavB",&dijet_lead_btagDeepFlavB,"dijet_lead_btagDeepFlavB/F");

  tout->Branch("dijet_sublead_pt",&dijet_sublead_pt,"dijet_sublead_pt/F");
  tout->Branch("dijet_sublead_eta",&dijet_sublead_eta,"dijet_sublead_eta/F");
  tout->Branch("dijet_sublead_phi",&dijet_sublead_phi,"dijet_sublead_phi/F");
  tout->Branch("dijet_sublead_mass",&dijet_sublead_mass,"dijet_sublead_mass/F");
  tout->Branch("dijet_sublead_btagDeepFlavB",&dijet_sublead_btagDeepFlavB,"dijet_sublead_btagDeepFlavB/F");

  tout->Branch("dijet_pt",&dijet_pt,"dijet_pt/F");
  tout->Branch("dijet_eta",&dijet_eta,"dijet_eta/F");  
  tout->Branch("dijet_phi",&dijet_phi,"dijet_phi/F");  
  tout->Branch("dijet_mass",&dijet_mass,"dijet_mass/F");  
  tout->Branch("dijet_dR",&dijet_dR,"dijet_dR/F"); 

  tout->Branch("year",&year_out,"year/I");
  tout->Branch("weight_central",&weight_central,"weight_central/F");
  tout->Branch("weight_central_initial",&weight_central_initial,"weight_central_initial/F");
  tout->Branch("weight_central_no_lumi",&weight_central_no_lumi,"weight_central_no_lumi/F");
  tout->Branch("event",&eventNum,"event/I");
  tout->Branch("process_id",&process_id,"process_id/I");

  if (year=="2016nonAPV" || year=="2016APV") year_out = 2016;
  else if (year=="2017") year_out = 2017;
  else if (year=="2018") year_out = 2018;
  else year_out==0;
  
  if (isMC) {
    tout->Branch("LeadPhoton_genPartFlav",&LeadPhoton_genPartFlav,"LeadPhoton_genPartFlav/I");
    tout->Branch("SubleadPhoton_genPartFlav",&SubleadPhoton_genPartFlav,"SubleadPhoton_genPartFlav/I");
    tout->Branch("n_gen_matched_jets",&n_gen_matched_jets,"n_gen_matched_jets/I");
    tout->Branch("n_gen_matched_in_dijet",&n_gen_matched_in_dijet,"n_gen_matched_in_dijet/I");
    tout->Branch("dijet_lead_gen_match",&dijet_lead_gen_match,"dijet_lead_gen_match/B");
    tout->Branch("dijet_sublead_gen_match",&dijet_sublead_gen_match,"dijet_sublead_gen_match/B");
    tout->Branch("GenHiggs_pt",&GenHiggs_pt,"GenHiggs_pt/F");
    tout->Branch("GenHiggs_eta",&GenHiggs_eta,"GenHiggs_eta/F");
    tout->Branch("GenHiggs_phi",&GenHiggs_phi,"GenHiggs_phi/F");
    tout->Branch("GenHiggs_mass",&GenHiggs_mass,"GenHiggs_mass/F");
    tout->Branch("GenHiggs_dR",&GenHiggs_dR,"GenHiggs_dR/F");
    tout->Branch("GenY_pt",&GenY_pt,"GenY_pt/F");
    tout->Branch("GenY_eta",&GenY_eta,"GenY_eta/F");
    tout->Branch("GenY_phi",&GenY_phi,"GenY_phi/F");
    tout->Branch("GenY_mass",&GenY_mass,"GenY_mass/F");
    tout->Branch("GenY_dR",&GenY_dR,"GenY_dR/F");
    tout->Branch("GenX_pt",&GenX_pt,"GenX_pt/F");
    tout->Branch("GenX_eta",&GenX_eta,"GenX_eta/F");
    tout->Branch("GenX_phi",&GenX_phi,"GenX_phi/F");
    tout->Branch("GenX_mass",&GenX_mass,"GenX_mass/F");
    tout->Branch("GenX_dR",&GenX_dR,"GenX_dR/F");
    tout->Branch("GenBFromHiggs_1_pt",&GenBFromHiggs_1_pt,"GenBFromHiggs_1_pt/F");
    tout->Branch("GenBFromHiggs_1_eta",&GenBFromHiggs_1_eta,"GenBFromHiggs_1_eta/F");
    tout->Branch("GenBFromHiggs_1_phi",&GenBFromHiggs_1_phi,"GenBFromHiggs_1_phi/F");
    tout->Branch("GenBFromHiggs_1_mass",&GenBFromHiggs_1_mass,"GenBFromHiggs_1_mass/F");
    tout->Branch("GenBFromHiggs_2_pt",&GenBFromHiggs_2_pt,"GenBFromHiggs_2_pt/F");
    tout->Branch("GenBFromHiggs_2_eta",&GenBFromHiggs_2_eta,"GenBFromHiggs_2_eta/F");
    tout->Branch("GenBFromHiggs_2_phi",&GenBFromHiggs_2_phi,"GenBFromHiggs_2_phi/F");
    tout->Branch("GenBFromHiggs_2_mass",&GenBFromHiggs_2_mass,"GenBFromHiggs_2_mass/F");
  }

  // Define histo info maps
  map<TString, int> nbins { };
  map<TString, float> low { };
  map<TString, float> high { };
  map<TString, vector<float>> binsx { };
  map<TString, TString> title { };

  // Define histos
  H1(cutflow,20,0,20,"");
  H1(weight,1,0,1,"");
  H1(weight_full,1,0,1,"");
  histoDefinition(nbins, low, high, binsx, title);

  // Define RooDataSet's for fit
  /*
  RooRealVar mfit("mfit", "mfit", 150.0, 3000.0);
  RooRealVar roow("roow", "roow", 1.0);
  map<TString, RooDataSet> roods;
  for ( unsigned int imll=0; imll < mllbin.size(); imll++ ) {
    for ( unsigned int inb=0; inb < nbtag.size(); inb++ ) {
      for (unsigned int iMuDet = 0; iMuDet < MuDetRegion.size(); iMuDet++){
        TString dname = TString("d_") + mllbin[imll] + TString("_") + nbtag[inb] + TString("_") + MuDetRegion[iMuDet];
	TString slice = mllbin[imll] + TString("_") + nbtag[inb] + TString("_") + MuDetRegion[iMuDet];
	if ( fillRooDataSet )
	  roods.insert({slice, RooDataSet(dname,dname,RooArgSet(mfit,roow),WeightVar(roow))});
      }
    }
  }
  */

  // Define selection
  cout << "define selection" << endl;
  vector<TString> selection = { };
  selection.push_back("sel0"); // Skimming + HLT + Good PV
  selection.push_back("sel1"); // 2 high-pT ID muons
  selection.push_back("sel2"); // pT > 53 GeV && |eta| < 2.4 muons

  vector<TString> plot_names = { };
  plot_names.push_back("pfmet_pt");

  map<TString, TH1D*> histos;

  if ( PUWeight!=0 ) set_puWeights(); //here

  // Setting up JEC uncertainties
  JetCorrectionUncertainty* jec_unc = new JetCorrectionUncertainty(
    "../NanoCORE/Tools/jetcorr/data/"
    + gconf.jecEraMC 
    + "/"
    + gconf.jecEraMC
    + "_Uncertainty_AK4PFchs.txt"
  );

  int nEventsTotal = 0;
  int nDuplicates = 0;
  int nEventsChain = ch->GetEntries();
  TFile *currentFile = 0;
  TObjArray *listOfFiles = ch->GetListOfFiles();
  TIter fileIter(listOfFiles);
  tqdm bar;

  while ( (currentFile = (TFile*)fileIter.Next()) ) {
    TFile *file = TFile::Open( currentFile->GetTitle() );
    TTree *tree = (TTree*)file->Get("Events");
    TString filename(currentFile->GetTitle());

    tree->SetCacheSize(128*1024*1024);
    tree->SetCacheLearnEntries(100);
//    nt.SetYear(2018);

    nt.Init(tree);

    // Before any cuts
    int icutflow = 0;
    TString label = "Total";
    TString slicedlabel = label;
    for( unsigned int event = 0; event < tree->GetEntriesFast(); ++event) {
//    for( unsigned int event = 0; event < 1000; ++event) {
      nt.GetEntry(event);
      tree->LoadTree(event);

      nEventsTotal++;
      bar.progress(nEventsTotal, nEventsChain);

      float weight = 1.0;
      if ( isMC ) {
	weight = nt.genWeight();
//	if(removeSpikes && weight*factor>1e2) continue; //comment out for synchronizing

	// Apply PU reweight
/*        
	if ( PUWeight!=0 ) {
	  unsigned int nTrueInt = nt.Pileup_nTrueInt();
	  TString whichPUWeight = "central";
	  if ( PUWeight==2 ) whichPUWeight = "up";
	  else if ( PUWeight==-2 ) whichPUWeight = "down";
	  TString puyear = year;
	  if ( useOnlyRun2018B ) puyear = "2018B";
	  weight *= get_puWeight(nTrueInt, puyear, whichPUWeight);
          if (event <  100) cout << "genWeight after PU reWeight is: " << weight << endl;
	}
*/        

      }

      h_weight_full->Fill(0.5, weight*factor);

      unsigned int runnb = nt.run();
      unsigned int lumiblock = nt.luminosityBlock();
      unsigned long int evtnb = nt.event();
      int npv = nt.PV_npvs();

      // Apply Golden JSON

      if ( !isMC ) {
	      if ( !(goodrun(runnb, lumiblock)) )
	        continue;
	      if ( removeDataDuplicates ) {
	        DorkyEventIdentifier id(runnb, evtnb, lumiblock);
	        if ( is_duplicate(id) ){
	          ++nDuplicates;
	          continue;
	        }
	      }
      }
 // HLT in MC is comment out for synchronizing (same as HiggsDNA)
      if (!(isMC)){
        // HLT selection
        if ( (year=="2016nonAPV" || year=="2016APV") &&
            !( (tree->GetBranch("HLT_Diphoton30PV_18PV_R9Id_AND_IsoCaloId_AND_HE_R9Id_DoublePixelVeto_Mass55") ? nt.HLT_Diphoton30PV_18PV_R9Id_AND_IsoCaloId_AND_HE_R9Id_DoublePixelVeto_Mass55() : 0)
              || (tree->GetBranch("HLT_Diphoton30EB_18EB_R9Id_OR_IsoCaloId_AND_HE_R9Id_DoublePixelVeto_Mass55") ? nt.HLT_Diphoton30EB_18EB_R9Id_OR_IsoCaloId_AND_HE_R9Id_DoublePixelVeto_Mass55() : 0) ) ) continue;
        if ( (year=="2017") &&
            !( (tree->GetBranch("HLT_Diphoton30PV_18PV_R9Id_AND_IsoCaloId_AND_HE_R9Id_PixelVeto_Mass55") ? nt.HLT_Diphoton30PV_18PV_R9Id_AND_IsoCaloId_AND_HE_R9Id_PixelVeto_Mass55() : 0)  )  ) continue;
        if ( (year=="2018") &&
            !( (tree->GetBranch("HLT_Diphoton30_18_R9IdL_AND_HE_AND_IsoCaloId_NoPixelVeto") ? nt.HLT_Diphoton30_18_R9IdL_AND_HE_AND_IsoCaloId_NoPixelVeto() : 0) ) ) continue;
      }
      // do diphoton selection here
      Photons photons = getPhotons(); //sort by pt
      DiPhotons diphotons = DiPhotonPreselection(photons);

      if (diphotons.size() == 0 ) continue; 

      DiPhoton selectedDiPhoton = diphotons[0];
      Photons vector_photons={};
      vector_photons.push_back(selectedDiPhoton.leadPho);
      vector_photons.push_back(selectedDiPhoton.subleadPho);
      LeadPhoton_mvaID = selectedDiPhoton.leadPho.mvaID();
      SubleadPhoton_mvaID = selectedDiPhoton.subleadPho.mvaID();
      if (!(SubleadPhoton_mvaID>-0.7 && LeadPhoton_mvaID>-0.7)) continue;

      Electrons electrons = getElectrons(vector_photons);
      Muons muons = getMuons(vector_photons);
      if (electrons.size() != 0 ) continue; 
      if (muons.size() != 0 ) continue; 

      Jets jets = getJets(vector_photons); //sort by b score
      DiJets dijets = DiJetPreselection(jets);
      if (jets.size() < 2) continue; 
      DiJet selectedDiJet = dijets[0];

      if (dijets[0].p4.M()<50) continue;

      if (isMC){
        LeadPhoton_genPartFlav = int(selectedDiPhoton.leadPho.genPartFlav());
        SubleadPhoton_genPartFlav = int(selectedDiPhoton.subleadPho.genPartFlav());
        TLorentzVector GenHiggs, GenX, GenY;
        GenParts genparts = getGenParts();
        vector<TLorentzVector> gen_child_xyh, gen_child_ygg, gen_child_hbb;
        for (int igenpart=0; igenpart<genparts.size(); igenpart++)
          {
            if (genparts[igenpart].isxyh()) {
              GenX = genparts[igenpart].mother_p4(); 
              gen_child_xyh.push_back(genparts[igenpart].p4());
              GenX_pt = GenX.Pt();
              GenX_eta = GenX.Eta();
              GenX_phi = GenX.Phi();
              GenX_mass = GenX.M();
            }
            if (genparts[igenpart].isygg()) {
              GenY = genparts[igenpart].mother_p4(); 
              gen_child_ygg.push_back(genparts[igenpart].p4());
              GenY_pt = GenY.Pt();
              GenY_eta = GenY.Eta();
              GenY_phi = GenY.Phi();
              GenY_mass = GenY.M();
            }
            if (genparts[igenpart].ishbb()) {
              GenHiggs = genparts[igenpart].mother_p4();
              gen_child_hbb.push_back(genparts[igenpart].p4());
              GenHiggs_pt = GenHiggs.Pt();
              GenHiggs_eta = GenHiggs.Eta();
              GenHiggs_phi = GenHiggs.Phi();
              GenHiggs_mass = GenHiggs.M();
            }
          }
        TLorentzVector sort_GenPart;
//          if (gen_child_xyh[0].Pt()<gen_child_xyh[1].Pt()) {sort_GenPart = gen_child_xyh[0]; gen_child_xyh[0]= gen_child_xyh[1]; gen_child_xyh[1] = sort_GenPart;}
//          if (gen_child_ygg[0].Pt()<gen_child_ygg[1].Pt()) {sort_GenPart = gen_child_ygg[0]; gen_child_ygg[0]= gen_child_ygg[1]; gen_child_ygg[1] = sort_GenPart;}
        if (!abs(GenX_pt+999)<0.0001){
          GenX_dR = gen_child_xyh[0].DeltaR(gen_child_xyh[1]);
        }
        if (!abs(GenY_pt+999)<0.0001){
          GenY_dR = gen_child_ygg[0].DeltaR(gen_child_ygg[1]);
        }
        if (!abs(GenHiggs_pt+999)<0.0001){
          GenHiggs_dR = gen_child_hbb[0].DeltaR(gen_child_hbb[1]);
          if (gen_child_hbb[0].Pt()<gen_child_hbb[1].Pt()) {sort_GenPart = gen_child_hbb[0]; gen_child_hbb[0]= gen_child_hbb[1]; gen_child_hbb[1] = sort_GenPart;}
          GenBFromHiggs_1_pt = gen_child_hbb[0].Pt();
          GenBFromHiggs_1_eta = gen_child_hbb[0].Eta();
          GenBFromHiggs_1_phi = gen_child_hbb[0].Phi();
          GenBFromHiggs_1_mass = gen_child_hbb[0].M();
          GenBFromHiggs_2_pt = gen_child_hbb[1].Pt();
          GenBFromHiggs_2_eta = gen_child_hbb[1].Eta();
          GenBFromHiggs_2_phi = gen_child_hbb[1].Phi();
          GenBFromHiggs_2_mass = gen_child_hbb[1].M();
          if (selectedDiJet.leadJet.p4().DeltaR(gen_child_hbb[0])<=0.4 || selectedDiJet.leadJet.p4().DeltaR(gen_child_hbb[1])<=0.4) {dijet_lead_gen_match=true; n_gen_matched_in_dijet++;}
          if (selectedDiJet.subleadJet.p4().DeltaR(gen_child_hbb[0])<=0.4 || selectedDiJet.subleadJet.p4().DeltaR(gen_child_hbb[1])<=0.4) {dijet_sublead_gen_match=true; n_gen_matched_in_dijet++;}
          for (int ijet=0; ijet<jets.size(); ijet++)
            if (jets[ijet].p4().DeltaR(gen_child_hbb[0])<=0.4 || jets[ijet].p4().DeltaR(gen_child_hbb[1])<=0.4)
              n_gen_matched_jets++;
        }
      }

      TLorentzVector x_cand = selectedDiPhoton.p4 + selectedDiJet.p4;

      xcand_pt = x_cand.Pt();
      xcand_eta = x_cand.Eta();
      xcand_phi = x_cand.Phi();
      xcand_mass = x_cand.M();

      LeadPhoton_pt = selectedDiPhoton.leadPho.pt();
      LeadPhoton_eta = selectedDiPhoton.leadPho.eta();
      LeadPhoton_phi = selectedDiPhoton.leadPho.phi();
      LeadPhoton_mass = selectedDiPhoton.leadPho.mass();
      LeadPhoton_r9 = selectedDiPhoton.leadPho.r9();
      LeadPhoton_pixelSeed = selectedDiPhoton.leadPho.pixelSeed();
      LeadPhoton_sieie = selectedDiPhoton.leadPho.sieie();
      LeadPhoton_pfPhoIso03 = selectedDiPhoton.leadPho.phoIso();
      LeadPhoton_trkSumPtHollowConeDR03 = selectedDiPhoton.leadPho.trkIso();
      LeadPhoton_chargedHadronIso = selectedDiPhoton.leadPho.chargedHadIso();

      SubleadPhoton_pt = selectedDiPhoton.subleadPho.pt();
      SubleadPhoton_eta = selectedDiPhoton.subleadPho.eta();
      SubleadPhoton_phi = selectedDiPhoton.subleadPho.phi();
      SubleadPhoton_mass = selectedDiPhoton.subleadPho.mass();
      SubleadPhoton_r9 = selectedDiPhoton.subleadPho.r9();
      SubleadPhoton_pixelSeed = selectedDiPhoton.subleadPho.pixelSeed();
      SubleadPhoton_sieie = selectedDiPhoton.subleadPho.sieie();
      SubleadPhoton_pfPhoIso03 = selectedDiPhoton.subleadPho.phoIso();
      SubleadPhoton_trkSumPtHollowConeDR03 = selectedDiPhoton.subleadPho.trkIso();
      SubleadPhoton_chargedHadronIso = selectedDiPhoton.subleadPho.chargedHadIso();

      Diphoton_pt = selectedDiPhoton.p4.Pt();
      Diphoton_eta = selectedDiPhoton.p4.Eta();
      Diphoton_phi = selectedDiPhoton.p4.Phi();
      Diphoton_mass = selectedDiPhoton.p4.M();
      Diphoton_pt_mgg = Diphoton_pt/Diphoton_mass;
      Diphoton_dR = selectedDiPhoton.dR;

      n_jets = jets.size();
      dijet_lead_pt = selectedDiJet.leadJet.pt();
      dijet_lead_eta = selectedDiJet.leadJet.eta();
      dijet_lead_phi = selectedDiJet.leadJet.phi();
      dijet_lead_mass = selectedDiJet.leadJet.mass();
      dijet_lead_btagDeepFlavB = selectedDiJet.leadJet.btagDeepFlavB();
      dijet_sublead_pt = selectedDiJet.subleadJet.pt();
      dijet_sublead_eta = selectedDiJet.subleadJet.eta();
      dijet_sublead_phi = selectedDiJet.subleadJet.phi();
      dijet_sublead_mass = selectedDiJet.subleadJet.mass();
      dijet_sublead_btagDeepFlavB = selectedDiJet.subleadJet.btagDeepFlavB();
      dijet_pt = selectedDiJet.p4.Pt();
      dijet_eta = selectedDiJet.p4.Eta();
      dijet_phi = selectedDiJet.p4.Phi();
      dijet_mass = selectedDiJet.p4.M();
      dijet_dR = selectedDiJet.dR;

      weight_central = weight*factor;
      eventNum = evtnb;

      count_test++;

      h_LeadPhoton_sieie->Fill(LeadPhoton_sieie);
      h_LeadPhoton_pfPhoIso03->Fill(LeadPhoton_pfPhoIso03);
      h_LeadPhoton_chargedHadronIso->Fill(LeadPhoton_chargedHadronIso);
      h_LeadPhoton_trkSumPtHollowConeDR03->Fill(LeadPhoton_trkSumPtHollowConeDR03);
      h_SubleadPhoton_sieie->Fill(SubleadPhoton_sieie);
      h_SubleadPhoton_pfPhoIso03->Fill(SubleadPhoton_pfPhoIso03);
      h_SubleadPhoton_chargedHadronIso->Fill(SubleadPhoton_chargedHadronIso);
      h_SubleadPhoton_trkSumPtHollowConeDR03->Fill(SubleadPhoton_trkSumPtHollowConeDR03);

      weight_central_initial = weight;
      weight_central_no_lumi = weight*factor/lumi;

      tout->Fill();
      h_weight->Fill(0.5, weight*factor);
//      h_weight->Fill(0.5);
    } // Event loop

    delete file;

  } // File loop
    
  bar.finish();
  cout << "nTotal: " << h_weight_full->GetBinContent(1) << ", nPass: " << h_weight->GetBinContent(1) << ", eff: " << h_weight->GetBinContent(1)/h_weight_full->GetBinContent(1) << endl;
  cout << endl;

  if ( muonSF!=0 ) {
    reset_muonRecoSF();
    reset_muonIDSF();
    reset_muonIsoSF();
  }
  if ( triggerSF!=0 ) reset_triggerSF();
  if ( bTagSF!=0 ) reset_bTagEff();

  if ( removeDataDuplicates )
    cout << "Number of duplicates found: " << nDuplicates << endl;

  // Avoid histograms with unphysical negative bin content (due to negative GEN weights)
  map<TString, TH1D*>::iterator it;
  for ( it = histos.begin(); it != histos.end(); it++ ) {
    for ( unsigned int b=1; b<(it->second)->GetNbinsX()+1; b++ ) { 
      if ( (it->second)->GetBinContent(b)<0.0) {
	(it->second)->SetBinContent(b,0.0);
	(it->second)->SetBinError(b,0.0);
      }
    }
  }
  txtout.close();
  //tout->Write();
  fout->Write();
  //if ( fillRooDataSet ) {
  //  fout->cd();
  //  for (const auto& d : roods )
  //    d.second.Write();
  //}
  fout->Close();

  return 0;
}
