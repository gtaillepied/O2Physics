// Copyright 2019-2020 CERN and copyright holders of ALICE O2.
// See https://alice-o2.web.cern.ch/copyright for details of the copyright holders.
// All rights not expressly granted are reserved.
//
// This software is distributed under the terms of the GNU General Public
// License v3 (GPL Version 3), copied verbatim in the file "COPYING".
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

/// \file k1analysis.cxx
/// \brief Reconstruction of track-track decay resonance candidates
///
///
/// \author Bong-Hwi Lim <bong-hwi.lim@cern.ch>

#include <TLorentzVector.h>

#include "Common/DataModel/PIDResponse.h"
#include "Common/DataModel/Centrality.h"
#include "Common/DataModel/EventSelection.h"
#include "Framework/AnalysisTask.h"
#include "Framework/ASoAHelpers.h"
#include "Framework/runDataProcessing.h"
#include "PWGLF/DataModel/LFResonanceTables.h"
#include "DataFormatsParameters/GRPObject.h"

using namespace o2;
using namespace o2::framework;
using namespace o2::framework::expressions;
using namespace o2::soa;

struct k1analysis {
  enum k1Type { kMatterPos = 1,
                kMatterNeg,
                kAntiPos,
                kAntiNeg,
                kMatterPos_Mix,
                kMatterNeg_Mix,
                kAntiPos_Mix,
                kAntiNeg_Mix,
                kNk1Type };
  enum k892Type {
    kK892Matter = 1,
    kK892Anti,
    kK892Matter_Mix,
    kK892Anti_Mix,
    kNk892Type
  };
  enum kMCType {
    kInput = 1,
    kRecon, // signal loss can be added.
    kNkMCType
  };
  SliceCache cache;
  Preslice<aod::ResoTracks> perRCol = aod::resodaughter::resoCollisionId;
  Preslice<aod::Tracks> perCollision = aod::track::collisionId;
  HistogramRegistry histos{"histos", {}, OutputObjHandlingPolicy::AnalysisObject};

  ///// Configurables
  /// Event Mixing
  Configurable<int> nEvtMixing{"nEvtMixing", 5, "Number of events to mix"};
  ConfigurableAxis CfgVtxBins{"CfgVtxBins", {VARIABLE_WIDTH, -10.0f, -8.f, -6.f, -4.f, -2.f, 0.f, 2.f, 4.f, 6.f, 8.f, 10.f}, "Mixing bins - z-vertex"};
  ConfigurableAxis CfgMultBins{"CfgMultBins", {VARIABLE_WIDTH, 0.0f, 20.0f, 40.0f, 60.0f, 80.0f, 100.0f, 200.0f, 99999.f}, "Mixing bins - multiplicity"};
  /// Pre-selection cuts
  Configurable<double> cMinPtcut{"cMinPtcut", 0.15, "Track minium pt cut"};
  /// DCA Selections
  // DCAr to PV
  Configurable<double> cMaxDCArToPVcut{"cMaxDCArToPVcut", 0.5, "Track DCAr cut to PV Maximum"};
  // DCAz to PV
  Configurable<double> cMaxDCAzToPVcut{"cMaxDCAzToPVcut", 2.0, "Track DCAz cut to PV Maximum"};
  Configurable<double> cMinDCAzToPVcut{"cMinDCAzToPVcut", 0.0, "Track DCAz cut to PV Minimum"};
  /// PID Selections
  Configurable<double> cMaxTPCnSigmaPion{"cMaxTPCnSigmaPion", 2.0, "TPC nSigma cut for Pion"};                    // TPC
  Configurable<double> cMaxTPCnSigmaPion_bach{"cMaxTPCnSigmaPion_bach", 2.0, "TPC nSigma cut for bachelor Pion"}; // TPC
  Configurable<double> cMaxTOFnSigmaPion{"cMaxTOFnSigmaPion", 2.0, "TOF nSigma cut for Pion"};                    // TOF
  Configurable<double> cMaxTOFnSigmaPion_bach{"cMaxTOFnSigmaPion_bach", 2.0, "TOF nSigma cut for Bachelor Pion"}; // TOF
  // Kaon
  Configurable<std::vector<double>> kaonTPCPIDpTintv{"kaonTPCPIDpTintv", {999.}, "pT intervals for Kaon TPC PID cuts"};
  Configurable<std::vector<double>> kaonTPCPIDcuts{"kaonTPCPIDcuts", {2}, "nSigma list for Kaon TPC PID cuts"};
  Configurable<std::vector<double>> kaonTOFPIDpTintv{"kaonTOFPIDpTintv", {999.}, "pT intervals for Kaon TOF PID cuts"};
  Configurable<std::vector<double>> kaonTOFPIDcuts{"kaonTOFPIDcuts", {2}, "nSigma list for Kaon TOF PID cuts"};

  // bachelor pion TOF PID?
  Configurable<int> cDoTOFPID{"cDoTOFPID", 1, "Do TOF PID"};

  // K(892)0 selection
  Configurable<double> cK892masswindow{"cK892masswindow", 0.1, "K(892)0 inv mass selection window"};
  Configurable<double> cPiPiMin{"cPiPiMin", 0, "Pion pair inv mass selection minimum"};
  Configurable<double> cPiPiMax{"cPiPiMax", 999, "Pion pair inv mass selection maximum"};

  // K1 selection
  Configurable<double> cK1MaxRap{"cK1MaxRap", 0.5, "K1 maximum rapidity"};
  Configurable<double> cK1MinRap{"cK1MinRap", -0.5, "K1 minimum rapidity"};

  void init(o2::framework::InitContext&)
  {
    std::vector<double> centBinning = {0., 1., 5., 10., 15., 20., 25., 30., 35., 40., 45., 50., 55., 60., 65., 70., 80., 90., 100.};
    AxisSpec centAxis = {centBinning, "V0M (%)"};
    AxisSpec multAxis = {0, 0, 100, "V0M (%)"}; // for future
    AxisSpec ptAxis = {200, 0, 20, "#it{p}_{T} (GeV/#it{c})"};
    AxisSpec invMassAxis = {900, 0.6, 1.5, "Invariant Mass (GeV/#it{c}^2)"};        // K(892)0
    AxisSpec invMassAxisReso = {1600, 0.9f, 2.5f, "Invariant Mass (GeV/#it{c}^2)"}; // K1
    AxisSpec invMassAxisScan = {250, 0, 2.5, "Invariant Mass (GeV/#it{c}^2)"};      // For selection
    AxisSpec pidQAAxis = {130, -6.5, 6.5};
    AxisSpec dataTypeAxis = {9, 0, 9, "Histogram types"};
    AxisSpec mcTypeAxis = {4, 0, 4, "Histogram types"};
    AxisSpec tempmultAxis = {3000, 0, 3000, "Raw Multiplicity"};

    // Mass QA (quick check)
    histos.add("k892invmass", "Invariant mass of K(892)0", HistType::kTH1F, {invMassAxis});
    histos.add("k1invmass", "Invariant mass of K1(1270)pm", HistType::kTH1F, {invMassAxisReso});

    // pT QA
    histos.add("QAbefore/trkpT_pi", "pT distribution of pion track candidates", HistType::kTH1F, {ptAxis});
    histos.add("QAbefore/trkpT_ka", "pT distribution of kaon track candidates", HistType::kTH1F, {ptAxis});
    histos.add("QAbefore/trkpT_pi_bach", "pT distribution of bachelor pion track candidates", HistType::kTH1F, {ptAxis});
    histos.add("QAafter/trkpT_pi", "pT distribution of pion track candidates", HistType::kTH1F, {ptAxis});
    histos.add("QAafter/trkpT_ka", "pT distribution of kaon track candidates", HistType::kTH1F, {ptAxis});
    histos.add("QAafter/trkpT_pi_bach", "pT distribution of bachelor pion track candidates", HistType::kTH1F, {ptAxis});
    // PID QA before cuts
    histos.add("QAbefore/TOF_TPC_Map_pi", "TOF + TPC Combined PID for Pion;#sigma_{TOF}^{Pion};#sigma_{TPC}^{Pion}", {HistType::kTH2F, {pidQAAxis, pidQAAxis}});
    histos.add("QAbefore/TOF_Nsigma_pi", "TOF NSigma for Pion;#it{p}_{T} (GeV/#it{c});#sigma_{TOF}^{Pion};", {HistType::kTH2F, {ptAxis, pidQAAxis}});
    histos.add("QAbefore/TPC_Nsigma_pi", "TPC NSigma for Pion;#it{p}_{T} (GeV/#it{c});#sigma_{TPC}^{Pion};", {HistType::kTH2F, {ptAxis, pidQAAxis}});
    histos.add("QAbefore/TOF_TPC_Map_ka", "TOF + TPC Combined PID for Pion;#sigma_{TOF}^{Kaon};#sigma_{TPC}^{Kaon}", {HistType::kTH2F, {pidQAAxis, pidQAAxis}});
    histos.add("QAbefore/TOF_Nsigma_ka", "TOF NSigma for Pion;#it{p}_{T} (GeV/#it{c});#sigma_{TOF}^{Kaon};", {HistType::kTH2F, {ptAxis, pidQAAxis}});
    histos.add("QAbefore/TPC_Nsigmaka", "TPC NSigma for Kaon;#it{p}_{T} (GeV/#it{c});#sigma_{TPC}^{Kaon};", {HistType::kTH2F, {ptAxis, pidQAAxis}});
    histos.add("QAbefore/TOF_TPC_Map_pi_bach", "TOF + TPC Combined PID for Pion;#sigma_{TOF}^{Pion};#sigma_{TPC}^{Pion}", {HistType::kTH2F, {pidQAAxis, pidQAAxis}});
    histos.add("QAbefore/TOF_Nsigma_pi_bach", "TOF NSigma for Pion;#it{p}_{T} (GeV/#it{c});#sigma_{TOF}^{Pion};", {HistType::kTH2F, {ptAxis, pidQAAxis}});
    histos.add("QAbefore/TPC_Nsigma_pi_bach", "TPC NSigma for Pion;#it{p}_{T} (GeV/#it{c});#sigma_{TPC}^{Pion};", {HistType::kTH2F, {ptAxis, pidQAAxis}});
    // PID QA after cuts
    histos.add("QAafter/TOF_TPC_Map_pi", "TOF + TPC Combined PID for Pion;#sigma_{TOF}^{Pion};#sigma_{TPC}^{Pion}", {HistType::kTH2F, {pidQAAxis, pidQAAxis}});
    histos.add("QAafter/TOF_Nsigma_pi", "TOF NSigma for Pion;#it{p}_{T} (GeV/#it{c});#sigma_{TOF}^{Pion};", {HistType::kTH2F, {ptAxis, pidQAAxis}});
    histos.add("QAafter/TPC_Nsigma_pi", "TPC NSigma for Pion;#it{p}_{T} (GeV/#it{c});#sigma_{TPC}^{Pion};", {HistType::kTH2F, {ptAxis, pidQAAxis}});
    histos.add("QAafter/TOF_TPC_Map_ka", "TOF + TPC Combined PID for Pion;#sigma_{TOF}^{Kaon};#sigma_{TPC}^{Kaon}", {HistType::kTH2F, {pidQAAxis, pidQAAxis}});
    histos.add("QAafter/TOF_Nsigma_ka", "TOF NSigma for Pion;#it{p}_{T} (GeV/#it{c});#sigma_{TOF}^{Kaon};", {HistType::kTH2F, {ptAxis, pidQAAxis}});
    histos.add("QAafter/TPC_Nsigmaka", "TPC NSigma for Kaon;#it{p}_{T} (GeV/#it{c});#sigma_{TPC}^{Kaon};", {HistType::kTH2F, {ptAxis, pidQAAxis}});
    histos.add("QAafter/TOF_TPC_Map_pi_bach", "TOF + TPC Combined PID for Pion;#sigma_{TOF}^{Pion};#sigma_{TPC}^{Pion}", {HistType::kTH2F, {pidQAAxis, pidQAAxis}});
    histos.add("QAafter/TOF_Nsigma_pi_bach", "TOF NSigma for Pion;#it{p}_{T} (GeV/#it{c});#sigma_{TOF}^{Pion};", {HistType::kTH2F, {ptAxis, pidQAAxis}});
    histos.add("QAafter/TPC_Nsigma_pi_bach", "TPC NSigma for Pion;#it{p}_{T} (GeV/#it{c});#sigma_{TPC}^{Pion};", {HistType::kTH2F, {ptAxis, pidQAAxis}});

    histos.add("QAMCbefore/InvMass_piK_pipi", "Invariant mass of pion + kaon and pion+pion;Invariant Mass (GeV/#it{c}^{2});Invariant Mass (GeV/#it{c}^{2});", {HistType::kTH2F, {invMassAxisScan, invMassAxisScan}});

    // THnSparse for Invariant mass
    histos.add("THnK892invmass", "Invariant mass of K(892)0", HistType::kTHnSparseD, {dataTypeAxis, tempmultAxis, ptAxis, invMassAxis});          // TODO: multiplicity bin has to be updatde.
    histos.add("THnK1invmass", "Invariant mass of K(892)0 + pion", HistType::kTHnSparseD, {dataTypeAxis, tempmultAxis, ptAxis, invMassAxisReso}); // TODO: multiplicity bin has to be updatde.

    if (doprocessMC || doprocessMCLight) {
      // MC QA
      histos.add("THnK1invmassMC", "Invariant mass of MC K(892)0 + pion", HistType::kTHnSparseD, {dataTypeAxis, tempmultAxis, ptAxis, invMassAxisReso}); // TODO: multiplicity bin has to be updatde.
      /*
      histos.add("hTrueK892pt", "pT distribution of True MC K(892)0", HistType::kTH1F, {ptAxis});
      */
      histos.add("hReconK892pt", "pT distribution of Reconstructed MC K(892)0", HistType::kTH1F, {ptAxis});
      histos.add("hTrueK1pt", "pT distribution of True MC K1", HistType::kTH1F, {ptAxis});
      histos.add("hReconK1pt", "pT distribution of Reconstructed MC K1", HistType::kTH1F, {ptAxis});
      histos.add("QAMCafter/InvMass_piK_pipi", "Invariant mass of pion + kaon and pion+pion;Invariant Mass (GeV/#it{c}^{2});Invariant Mass (GeV/#it{c}^{2});", {HistType::kTH2F, {invMassAxisScan, invMassAxisScan}});
    }
  }

  double massKa = TDatabasePDG::Instance()->GetParticle(kKPlus)->Mass();
  double massPi = TDatabasePDG::Instance()->GetParticle(kPiPlus)->Mass();
  double massK892 = TDatabasePDG::Instance()->GetParticle(313)->Mass();

  template <typename TrackType>
  bool trackCut(const TrackType track)
  {
    // basic track cuts
    if (track.pt() < cMinPtcut)
      return false;
    if (track.dcaXY() > cMaxDCArToPVcut)
      return false;
    if (track.dcaZ() < cMinDCAzToPVcut || track.dcaZ() > cMaxDCAzToPVcut)
      return false;

    return true;
  }

  template <bool IsMC, bool IsMix, typename CollisionType, typename TracksType>
  void fillHistograms(const CollisionType& collision, const TracksType& dTracks1, const TracksType& dTracks2)
  {
    TLorentzVector lDecayDaughter1, lDecayDaughter2, lResonanceK892, lDecayDaughter_bach, lResonanceK1;
    bool isTrk1Selected{true}, isTrk2Selected{true}, isbTrkSelected{true}, isTrk1hasTOF{false}, isTrk2hasTOF{false}, isbTrkhasTOF{false}, isK892Anti{false};
    auto vKaonTPCPIDpTintv = static_cast<std::vector<double>>(kaonTPCPIDpTintv);
    auto vKaonTPCPIDcuts = static_cast<std::vector<double>>(kaonTPCPIDcuts);
    auto vKaonTOFPIDpTintv = static_cast<std::vector<double>>(kaonTOFPIDpTintv);
    auto vKaonTOFPIDcuts = static_cast<std::vector<double>>(kaonTOFPIDcuts);
    auto lengthOfkaonTPCPIDpTintv = static_cast<int>(vKaonTPCPIDpTintv.size());
    for (auto& [trk1, trk2] : combinations(CombinationsFullIndexPolicy(dTracks2, dTracks2))) {
      // Full index policy is needed to consider all possible combinations
      if (trk1.index() == trk2.index())
        continue;                        // We need to run (0,1), (1,0) pairs as well. but same id pairs are not needed.
      if (trk1.sign() * trk2.sign() > 0) // always choose opposite sign pairs
        continue;
      isK892Anti = false;
      //// Initialize variables
      // Trk1: Pion, Trk2: Kaon
      isTrk1Selected = true;
      isTrk2Selected = true;
      isTrk1hasTOF = false;
      isTrk2hasTOF = false;
      auto trk1ptPi = trk1.pt();
      auto trk1NSigmaPiTPC = trk1.tpcNSigmaPi();
      auto trk1NSigmaPiTOF = -999.;
      auto trk2ptKa = trk2.pt();
      auto trk2NSigmaKaTPC = trk2.tpcNSigmaKa();
      auto trk2NSigmaKaTOF = -999.;

      // apply the track cut
      if (!trackCut(trk1))
        continue;
      if (!trackCut(trk2))
        continue;

      // hasTOF?
      if ((trk1.tofPIDselectionFlag() & aod::resodaughter::kHasTOF) == aod::resodaughter::kHasTOF) {
        isTrk1hasTOF = true;
      }
      if ((trk2.tofPIDselectionFlag() & aod::resodaughter::kHasTOF) == aod::resodaughter::kHasTOF) {
        isTrk2hasTOF = true;
      }
      //// PID selections
      // For Pion candidate, we don't need to apply pT-dependent PID cuts
      if (std::abs(trk1NSigmaPiTPC) > cMaxTPCnSigmaPion)
        isTrk1Selected = false;
      if (isTrk1hasTOF) {
        trk1NSigmaPiTOF = trk1.tofNSigmaPi();
        if (std::abs(trk1NSigmaPiTOF) > cMaxTOFnSigmaPion)
          isTrk1Selected = false;
      }
      // For Kaon candidate, we need to apply pT-dependent PID cuts
      if (lengthOfkaonTPCPIDpTintv > 0) {
        for (int i = 0; i < lengthOfkaonTPCPIDpTintv; i++) {
          if (trk2ptKa < vKaonTPCPIDpTintv[i]) {
            if (std::abs(trk2NSigmaKaTPC) > vKaonTPCPIDcuts[i])
              isTrk2Selected = false;
          }
        }
      }
      if (isTrk2hasTOF) {
        trk2NSigmaKaTOF = trk2.tofNSigmaKa();
        if (lengthOfkaonTPCPIDpTintv > 0) {
          for (int i = 0; i < lengthOfkaonTPCPIDpTintv; i++) {
            if (trk2ptKa < vKaonTOFPIDpTintv[i]) {
              if (std::abs(trk2NSigmaKaTOF) > vKaonTOFPIDcuts[i])
                isTrk2Selected = false;
            }
          }
        }
      }

      //// QA plots before the selection
      //  --- PID QA Pion
      if constexpr (!IsMix) {
        histos.fill(HIST("QAbefore/TPC_Nsigma_pi"), trk1ptPi, trk1NSigmaPiTPC);
        if (isTrk1hasTOF) {
          histos.fill(HIST("QAbefore/TOF_Nsigma_pi"), trk1ptPi, trk1NSigmaPiTOF);
          histos.fill(HIST("QAbefore/TOF_TPC_Map_pi"), trk1NSigmaPiTOF, trk1NSigmaPiTPC);
        }
        //  --- PID QA Kaon
        histos.fill(HIST("QAbefore/TPC_Nsigmaka"), trk2ptKa, trk2NSigmaKaTPC);
        if (isTrk1hasTOF) {
          histos.fill(HIST("QAbefore/TOF_Nsigma_ka"), trk2ptKa, trk2NSigmaKaTOF);
          histos.fill(HIST("QAbefore/TOF_TPC_Map_ka"), trk2NSigmaKaTOF, trk2NSigmaKaTPC);
        }
        histos.fill(HIST("QAbefore/trkpT_pi"), trk1ptPi);
        histos.fill(HIST("QAbefore/trkpT_ka"), trk2ptKa);
      }

      //// Apply the selection
      if (!isTrk1Selected || !isTrk2Selected)
        continue;

      //// QA plots after the selection
      //  --- PID QA Pion
      if constexpr (!IsMix) {
        histos.fill(HIST("QAafter/TPC_Nsigma_pi"), trk1ptPi, trk1NSigmaPiTPC);
        if (isTrk1hasTOF) {
          histos.fill(HIST("QAafter/TOF_Nsigma_pi"), trk1ptPi, trk1NSigmaPiTOF);
          histos.fill(HIST("QAafter/TOF_TPC_Map_pi"), trk1NSigmaPiTOF, trk1NSigmaPiTPC);
        }
        //  --- PID QA Kaon
        histos.fill(HIST("QAafter/TPC_Nsigmaka"), trk2ptKa, trk2NSigmaKaTPC);
        if (isTrk1hasTOF) {
          histos.fill(HIST("QAafter/TOF_Nsigma_ka"), trk2ptKa, trk2NSigmaKaTOF);
          histos.fill(HIST("QAafter/TOF_TPC_Map_ka"), trk2NSigmaKaTOF, trk2NSigmaKaTPC);
        }
        histos.fill(HIST("QAafter/trkpT_pi"), trk1ptPi);
        histos.fill(HIST("QAafter/trkpT_ka"), trk2ptKa);
      }

      //// Resonance reconstruction
      lDecayDaughter1.SetXYZM(trk1.px(), trk1.py(), trk1.pz(), massPi);
      lDecayDaughter2.SetXYZM(trk2.px(), trk2.py(), trk2.pz(), massKa);
      lResonanceK892 = lDecayDaughter1 + lDecayDaughter2;

      if (trk2.sign() > 0)
        isK892Anti = true;
      if constexpr (!IsMix) {
        histos.fill(HIST("k892invmass"), lResonanceK892.M()); // quick check
        histos.fill(HIST("THnK892invmass"), (isK892Anti) ? kK892Anti : kK892Matter, collision.multV0M(), lResonanceK892.Pt(), lResonanceK892.M());
      }
      // Mass window cut
      if (std::abs(lResonanceK892.M() - massK892) > cK892masswindow)
        continue;
      // Add one more track loop for K1 reconstruction
      for (auto bTrack : dTracks1) {

        // ID cut
        if (bTrack.index() == trk1.index() || bTrack.index() == trk2.index())
          continue;

        // Track cut
        if (!trackCut(bTrack))
          continue;

        isbTrkSelected = true;
        auto bTrkPt = bTrack.pt();
        auto bTrkTPCnSigmaPi = bTrack.tpcNSigmaPi();
        auto bTrack_TOFnSigma = -99.0;
        isbTrkhasTOF = false;
        if ((bTrack.tofPIDselectionFlag() & aod::resodaughter::kHasTOF) == aod::resodaughter::kHasTOF) {
          isbTrkhasTOF = true;
        }

        // PID selection
        if (std::abs(bTrkTPCnSigmaPi) > cMaxTPCnSigmaPion_bach)
          isbTrkSelected = false;
        if (cDoTOFPID && isbTrkhasTOF) {
          bTrack_TOFnSigma = bTrack.tofNSigmaPi();
          if (std::abs(bTrack_TOFnSigma) > cMaxTOFnSigmaPion_bach)
            isbTrkSelected = false;
        }
        if constexpr (!IsMix) {
          histos.fill(HIST("QAbefore/trkpT_pi_bach"), bTrkPt);
          //  --- PID QA Pion
          histos.fill(HIST("QAbefore/TPC_Nsigma_pi_bach"), bTrkPt, bTrkTPCnSigmaPi);
          if (isTrk1hasTOF) {
            histos.fill(HIST("QAbefore/TOF_Nsigma_pi_bach"), bTrkPt, bTrack_TOFnSigma);
            histos.fill(HIST("QAbefore/TOF_TPC_Map_pi_bach"), bTrack_TOFnSigma, bTrkTPCnSigmaPi);
          }
        }

        if (!isbTrkSelected) // bachelor track selection
          continue;

        if constexpr (!IsMix) {
          histos.fill(HIST("QAafter/trkpT_pi_bach"), bTrkPt);
          //  --- PID QA Pion
          histos.fill(HIST("QAafter/TPC_Nsigma_pi_bach"), bTrkPt, bTrkTPCnSigmaPi);
          if (isTrk1hasTOF) {
            histos.fill(HIST("QAafter/TOF_Nsigma_pi_bach"), bTrkPt, bTrack_TOFnSigma);
            histos.fill(HIST("QAafter/TOF_TPC_Map_pi_bach"), bTrack_TOFnSigma, bTrkTPCnSigmaPi);
          }
        }

        // K1 reconstruction
        lDecayDaughter_bach.SetXYZM(bTrack.px(), bTrack.py(), bTrack.pz(), massPi);
        lResonanceK1 = lResonanceK892 + lDecayDaughter_bach;

        // Rapidity cut
        if (lResonanceK1.Rapidity() > cK1MaxRap || lResonanceK1.Rapidity() < cK1MinRap)
          continue;

        TLorentzVector tempPiPi = lDecayDaughter1 + lDecayDaughter_bach;
        if constexpr (!IsMix) {
          histos.fill(HIST("QAMCbefore/InvMass_piK_pipi"), lResonanceK892.M(), tempPiPi.M());
        }
        if (tempPiPi.M() < cPiPiMin || tempPiPi.M() > cPiPiMax)
          continue;

        if constexpr (!IsMix) {
          if (bTrack.sign() > 0) {
            // bachelor pi+
            if (!isK892Anti) {
              histos.fill(HIST("k1invmass"), lResonanceK1.M()); // quick check
              histos.fill(HIST("THnK1invmass"), kMatterPos, collision.multV0M(), lResonanceK1.Pt(), lResonanceK1.M());
            } else {
              histos.fill(HIST("THnK1invmass"), kAntiPos, collision.multV0M(), lResonanceK1.Pt(), lResonanceK1.M());
            }
          } else {
            // bachelor pi-
            if (!isK892Anti) {
              histos.fill(HIST("THnK1invmass"), kMatterNeg, collision.multV0M(), lResonanceK1.Pt(), lResonanceK1.M());
            } else {
              histos.fill(HIST("k1invmass"), lResonanceK1.M()); // quick check
              histos.fill(HIST("THnK1invmass"), kAntiNeg, collision.multV0M(), lResonanceK1.Pt(), lResonanceK1.M());
            }
          }
        } else {
          if (bTrack.sign() > 0) {
            // bachelor pi+
            if (!isK892Anti) {
              histos.fill(HIST("k1invmass"), lResonanceK1.M()); // quick check
              histos.fill(HIST("THnK1invmass"), kMatterPos_Mix, collision.multV0M(), lResonanceK1.Pt(), lResonanceK1.M());
            } else {
              histos.fill(HIST("THnK1invmass"), kAntiPos_Mix, collision.multV0M(), lResonanceK1.Pt(), lResonanceK1.M());
            }
          } else {
            // bachelor pi-
            if (!isK892Anti) {
              histos.fill(HIST("THnK1invmass"), kMatterNeg_Mix, collision.multV0M(), lResonanceK1.Pt(), lResonanceK1.M());
            } else {
              histos.fill(HIST("k1invmass"), lResonanceK1.M()); // quick check
              histos.fill(HIST("THnK1invmass"), kAntiNeg_Mix, collision.multV0M(), lResonanceK1.Pt(), lResonanceK1.M());
            }
          }
        }
        // MC
        if constexpr (IsMC) {
          if (abs(trk1.pdgCode()) != kPiPlus || abs(trk2.pdgCode()) != kKPlus)
            continue;
          auto mother1 = trk1.motherId();
          auto mother2 = trk2.motherId();
          if (mother1 == mother2) {             // Same mother
            if (abs(trk1.motherPDG()) == 313) { // k892(0)
              histos.fill(HIST("hReconK892pt"), lResonanceK892.Pt());
              if (abs(bTrack.pdgCode()) != kPiPlus)
                continue;
              // TODO: check if the 313 and bTrack have the same mother
              if (abs(bTrack.motherPDG()) == 10323) {
                histos.fill(HIST("hReconK1pt"), lResonanceK1.Pt());
                histos.fill(HIST("THnK1invmassMC"), kRecon, collision.multV0M(), lResonanceK1.Pt(), lResonanceK1.M()); // TODO: multV0M has to be updatde.
                histos.fill(HIST("QAMCafter/InvMass_piK_pipi"), lResonanceK892.M(), tempPiPi.M());
              }
            }
          }
        }
      }
    }
  }

  void processData(aod::ResoCollisions& collisions,
                   aod::ResoTracks const& resotracks)
  {
    LOGF(debug, "[DATA] Processing %d collisions", collisions.size());
    for (auto& collision : collisions) {
      Partition<aod::ResoTracks> selectedTracks = (o2::aod::track::pt > static_cast<float_t>(cMinPtcut)) && (nabs(o2::aod::track::dcaZ) > static_cast<float_t>(cMinDCAzToPVcut)) && (nabs(o2::aod::track::dcaZ) < static_cast<float_t>(cMaxDCAzToPVcut)) && (nabs(o2::aod::track::dcaXY) < static_cast<float_t>(cMaxDCArToPVcut)); // Basic DCA cuts
      selectedTracks.bindTable(resotracks);
      auto colTracks = selectedTracks->sliceByCached(aod::resodaughter::resoCollisionId, collision.globalIndex(), cache);
      fillHistograms<false, false>(collision, colTracks, colTracks);
    }
  }
  PROCESS_SWITCH(k1analysis, processData, "Process Event for data", false);

  void processDataLight(aod::ResoCollision& collision,
                        aod::ResoTracks const& resotracks)
  {
    fillHistograms<false, false>(collision, resotracks, resotracks);
  }
  PROCESS_SWITCH(k1analysis, processDataLight, "Process Event for data without Partitioning", true);

  void processMC(aod::ResoCollisions& collisions,
                 soa::Join<aod::ResoTracks, aod::ResoMCTracks> const& resotracks, aod::McParticles const& mcParticles)
  {
    LOGF(debug, "[MC] MC events: %d", collisions.size());
    for (auto& collision : collisions) {
      Partition<soa::Join<aod::ResoTracks, aod::ResoMCTracks>> selectedTracks = (o2::aod::track::pt > static_cast<float_t>(cMinPtcut)) && (nabs(o2::aod::track::dcaZ) > static_cast<float_t>(cMinDCAzToPVcut)) && (nabs(o2::aod::track::dcaZ) < static_cast<float_t>(cMaxDCAzToPVcut)) && (nabs(o2::aod::track::dcaXY) < static_cast<float_t>(cMaxDCArToPVcut)); // Basic DCA cuts
      selectedTracks.bindTable(resotracks);
      auto colTracks = selectedTracks->sliceByCached(aod::resodaughter::resoCollisionId, collision.globalIndex(), cache);
      fillHistograms<true, false>(collision, colTracks, colTracks);
    }

    // Not related to the real collisions
    for (auto& part : mcParticles) {             // loop over all MC particles
      if (abs(part.pdgCode()) == 10323) {        // K1
        if (part.y() > 0.5 || part.y() < -0.5) { // rapidity cut
          continue;
        }
        bool pass1 = false;
        bool pass2 = false;
        for (auto& dau : part.daughters_as<aod::McParticles>()) {
          if (abs(dau.pdgCode()) == 313) { // At least one decay to K892(0)
            pass2 = true;
          }
          if (abs(dau.pdgCode()) == kPiPlus) { // At least one decay to Pion
            pass1 = true;
          }
        }
        if (!pass1 || !pass2) // If we have both decay products
          continue;
        histos.fill(HIST("hTrueK1pt"), part.pt());
      }
    }
  }
  PROCESS_SWITCH(k1analysis, processMC, "Process Event for MC", false);

  void processMCLight(aod::ResoCollision& collision,
                      soa::Join<aod::ResoTracks, aod::ResoMCTracks> const& resotracks, aod::McParticles const& mcParticles)
  {
    fillHistograms<true, false>(collision, resotracks, resotracks);
  }
  PROCESS_SWITCH(k1analysis, processMCLight, "Process Event for MC", false);

  void processMCTrue(aod::ResoCollisions& collisions, aod::McParticles const& mcParticles)
  {
    // Not related to the real collisions
    for (auto& part : mcParticles) {             // loop over all MC particles
      if (abs(part.pdgCode()) == 10323) {        // K1
        if (part.y() > 0.5 || part.y() < -0.5) { // rapidity cut
          continue;
        }
        bool pass1 = false;
        bool pass2 = false;
        for (auto& dau : part.daughters_as<aod::McParticles>()) {
          if (abs(dau.pdgCode()) == 313) { // At least one decay to K892(0)
            pass2 = true;
          }
          if (abs(dau.pdgCode()) == kPiPlus) { // At least one decay to Pion
            pass1 = true;
          }
        }
        if (!pass1 || !pass2) // If we have both decay products
          continue;
        histos.fill(HIST("hTrueK1pt"), part.pt());
        // histos.fill(HIST("THnK1invmassMC"), kInput, collision.multV0M(), part.pt(), part.weight()); // TODO: multV0M has to be updatde.
      }
    }
  }
  PROCESS_SWITCH(k1analysis, processMCTrue, "Process Event for MC True", false);

  // Processing Event Mixing
  using BinningTypeVetZTPCtemp = ColumnBinningPolicy<aod::collision::PosZ, aod::resocollision::MultTPCtemp>; // TODO: MultTPCtemp has to be updatde.
  BinningTypeVetZTPCtemp colBinning{{CfgVtxBins, CfgMultBins}, true};
  void processME(o2::aod::ResoCollisions& collisions, aod::ResoTracks const& resotracks)
  {
    LOGF(debug, "Event Mixing Started");
    auto tracksTuple = std::make_tuple(resotracks);
    SameKindPair<aod::ResoCollisions, aod::ResoTracks, BinningTypeVetZTPCtemp> pairs{colBinning, nEvtMixing, -1, collisions, tracksTuple, &cache}; // -1 is the number of the bin to skip

    TLorentzVector lDecayDaughter1, lDecayDaughter2, lResonance;
    for (auto& [collision1, tracks1, collision2, tracks2] : pairs) {
      Partition<aod::ResoTracks> selectedTracks1 = (o2::aod::track::pt > static_cast<float_t>(cMinPtcut)) && (nabs(o2::aod::track::dcaZ) > static_cast<float_t>(cMinDCAzToPVcut)) && (nabs(o2::aod::track::dcaZ) < static_cast<float_t>(cMaxDCAzToPVcut)) && (nabs(o2::aod::track::dcaXY) < static_cast<float_t>(cMaxDCArToPVcut)); // Basic DCA cuts
      selectedTracks1.bindTable(tracks1);
      Partition<aod::ResoTracks> selectedTracks2 = (o2::aod::track::pt > static_cast<float_t>(cMinPtcut)) && (nabs(o2::aod::track::dcaZ) > static_cast<float_t>(cMinDCAzToPVcut)) && (nabs(o2::aod::track::dcaZ) < static_cast<float_t>(cMaxDCAzToPVcut)) && (nabs(o2::aod::track::dcaXY) < static_cast<float_t>(cMaxDCArToPVcut)); // Basic DCA cuts
      selectedTracks2.bindTable(tracks2);

      fillHistograms<false, true>(collision1, selectedTracks1, selectedTracks2);
    }
  };
  PROCESS_SWITCH(k1analysis, processME, "Process EventMixing", false);
};

WorkflowSpec defineDataProcessing(ConfigContext const& cfgc)
{
  return WorkflowSpec{adaptAnalysisTask<k1analysis>(cfgc, TaskName{"lf-k1analysis"})};
}
