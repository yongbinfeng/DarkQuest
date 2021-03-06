#include "G4_InsensitiveVolumes.C"
#include "G4_SensitiveDetectors.C"
#include "G4_Beamline.C"
#include "G4_Target.C"
#include "G4_EMCal.C"
R__LOAD_LIBRARY(libfun4all)
R__LOAD_LIBRARY(libg4detectors)
R__LOAD_LIBRARY(libg4testbench)
R__LOAD_LIBRARY(libg4eval)
R__LOAD_LIBRARY(libg4dst)
R__LOAD_LIBRARY(libdptrigger)
R__LOAD_LIBRARY(libevt_filter)
//R__LOAD_LIBRARY(libktracker)
using namespace std;

#include <iostream>
#include <sstream>
using namespace std;

int Fun4Sim(
	    const int nevent = 2000,
	    std::string ifile = "Brem_1.04_z500_600_eps_-6.4",
	    const int idLep = 11
    )
{
  const double target_coil_pos_z = -300;
  const double collimator_pos_z = -602.36;
  const int nmu = 1;

  const bool do_collimator = true;
  const bool do_target = true;
  const bool do_shielding = true;
  const bool do_fmag = true;
  const bool do_kmag = true;
  const bool do_absorber = true;
  const bool do_dphodo = true;
  const bool do_station1DC = false;       //station-1 drift chamber should be turned off by default

  const double target_l = 7.9; //cm
  const double target_z = (7.9-target_l)/2.; //cm
  const int use_g4steps = 1;

  const double FMAGSTR = -1.054;
  const double KMAGSTR = -0.951;

  recoConsts *rc = recoConsts::instance();
  rc->set_DoubleFlag("FMAGSTR", FMAGSTR);
  rc->set_DoubleFlag("KMAGSTR", KMAGSTR);
  rc->Print();

  // Alignment
  JobOptsSvc *jobopt_svc = JobOptsSvc::instance();
  jobopt_svc->init("run7_sim.opts");
  GeomSvc::UseDbSvc(true);
  GeomSvc *geom_svc = GeomSvc::instance();

  // Make the Server
  Fun4AllServer *se = Fun4AllServer::instance();
  se->Verbosity(0);

  // HepMC reader
  HepMCNodeReader *hr = new HepMCNodeReader();
  hr->set_particle_filter_on(true);
  hr->insert_particle_filter_pid(idLep);
  hr->insert_particle_filter_pid(idLep*-1);
  se->registerSubsystem(hr);

  // Fun4All G4 module
  PHG4Reco *g4Reco = new PHG4Reco();
  g4Reco->set_field_map(jobopt_svc->m_fMagFile+" "+
			jobopt_svc->m_kMagFile+" "+
			Form("%f",FMAGSTR) + " " +
			Form("%f",KMAGSTR) + " " +
			"5.0",
			PHFieldConfig::RegionalConst);
  g4Reco->SetWorldSizeX(1000); // size of the world - every detector has to fit in here
  g4Reco->SetWorldSizeY(1000);
  g4Reco->SetWorldSizeZ(5000);
  g4Reco->SetWorldShape("G4BOX"); // shape of our world - it is a box
  g4Reco->SetWorldMaterial("G4_AIR"); // this is what our world is filled with G4_Galactic, G4_AIR
  g4Reco->SetPhysicsList("FTFP_BERT"); // Geant4 Physics list to use

  SetupBeamline(g4Reco, do_collimator, collimator_pos_z); // // collimator, target and shielding between target and FMag
  if (do_target) {
    SetupTarget(g4Reco, target_coil_pos_z, target_l, target_z, use_g4steps, 0);
  }
  SetupInsensitiveVolumes(g4Reco, do_shielding, do_fmag, do_kmag, do_absorber); // insensitive elements of the spectrometer
  SetupSensitiveDetectors(g4Reco, do_dphodo, do_station1DC); // sensitive elements of the spectrometer
  SetupEMCal(g4Reco, "EMCal", 0., -110., 1930.); // emcal
  se->registerSubsystem(g4Reco);

  // save truth info to the Node Tree
  PHG4TruthSubsystem *truth = new PHG4TruthSubsystem();
  g4Reco->registerSubsystem(truth);

  // digitizer
  SQDigitizer* digitizer = new SQDigitizer("Digitizer", 0);
  digitizer->set_enable_st1dc(do_station1DC);
  digitizer->set_enable_dphodo(do_dphodo); 
  digitizer->registerEMCal("EMCal", 100);
  se->registerSubsystem(digitizer);

  // Trigger Emulator
  gSystem->Load("libdptrigger.so");
  DPTriggerAnalyzer* dptrigger = new DPTriggerAnalyzer();
  dptrigger->set_hit_container_choice("Vector");
  dptrigger->set_road_set_file_name(gSystem->ExpandPathName("$E1039_RESOURCE/trigger/trigger_67.txt"));
  se->registerSubsystem(dptrigger);

  // Event Filter
  EvtFilter *evt_filter = new EvtFilter();
  se->registerSubsystem(evt_filter);

  // tracking module
  // gSystem->Load("libktracker.so");
  // KalmanFastTrackingWrapper *ktracker = new KalmanFastTrackingWrapper();
  // ktracker->set_enable_event_reducer(true);
  // ktracker->set_DS_level(0);
  // ktracker->set_pattern_db_name(gSystem->ExpandPathName("$E1039_RESOURCE/dsearch/v1/pattern.root"));
  // se->registerSubsystem(ktracker);

  // VertexFit* vertexing = new VertexFit();
  // se->registerSubsystem(vertexing);

  // input 
  Fun4AllHepMCInputManager *in = new Fun4AllHepMCInputManager("HEPMCIN");
  //in->set_vertex_distribution_mean(0,0,target_coil_pos_z,0);
  se->registerInputManager(in);
  stringstream ssin; ssin << "$DIR_TOP/../../lhe/displaced_Aprime_Electrons/" << ifile << ".txt";
  //stringstream ssin; ssin << ifile << ".txt";
  in->fileopen(gSystem->ExpandPathName(ssin.str().c_str()));

  // DST output manager
  //stringstream ssout; ssout << ifile << "0_dst.root";
  stringstream ssout; ssout << "$DIR_TOP/macro/output_electrons_emcal/" << ifile << "0_dst.root";
  Fun4AllDstOutputManager *out = new Fun4AllDstOutputManager("DSTOUT", ssout.str().c_str());
  se->registerOutputManager(out);

  se->run(nevent);

  // finish job - close and save output files
  se->End();
  se->PrintTimer();

  // cleanup - delete the server and exit
  delete se;
  gSystem->Exit(0);
  return 0;
}

