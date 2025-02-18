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

/// \file candidateSelectorBplusToD0Pi.cxx
/// \brief B± → D0bar(D0) π± candidate selector
///
/// \author Antonio Palasciano <antonio.palasciano@cern.ch>, Università degli Studi di Bari & INFN, Sezione di Bari
/// \author Deepa Thomas <deepa.thomas@cern.ch>, UT Austin

#include "Framework/AnalysisTask.h"
#include "Framework/runDataProcessing.h"
#include "Framework/RunningWorkflowInfo.h"

#include "Common/Core/TrackSelectorPID.h"

#include "PWGHF/Core/SelectorCuts.h"
#include "PWGHF/DataModel/CandidateReconstructionTables.h"
#include "PWGHF/DataModel/CandidateSelectionTables.h"

using namespace o2;
using namespace o2::aod;
using namespace o2::framework;
using namespace o2::aod::hf_cand_bplus;
using namespace o2::analysis;
using namespace o2::aod::hf_cand_2prong;
using namespace o2::analysis::hf_cuts_bplus_to_d0_pi;

struct HfCandidateSelectorBplusToD0Pi {
  Produces<aod::HfSelBplusToD0Pi> hfSelBplusToD0PiCandidate;

  // Configurable<double> ptCandMin{"ptCandMin", 0., "Lower bound of candidate pT"};
  // Configurable<double> ptCandMax{"ptCandMax", 50., "Upper bound of candidate pT"};
  // Enable PID
  Configurable<bool> usePid{"usePid", true, "Bool to use or not the PID at filtering level"};
  Configurable<bool> acceptPIDNotApplicable{"acceptPIDNotApplicable", true, "Switch to accept Status::PIDNotApplicable [(NotApplicable for one detector) and (NotApplicable or Conditional for the other)] in PID selection"};
  // TPC PID
  Configurable<double> ptPidTpcMin{"ptPidTpcMin", 999, "Lower bound of track pT for TPC PID"};
  Configurable<double> ptPidTpcMax{"ptPidTpcMax", 9999, "Upper bound of track pT for TPC PID"};
  Configurable<double> nSigmaTpcMax{"nSigmaTpcMax", 5., "Nsigma cut on TPC only"};
  Configurable<double> nSigmaTpcCombinedMax{"nSigmaTpcCombinedMax", 5., "Nsigma cut on TPC combined with TOF"};
  // TOF PID
  Configurable<double> ptPidTofMin{"ptPidTofMin", 0.15, "Lower bound of track pT for TOF PID"};
  Configurable<double> ptPidTofMax{"ptPidTofMax", 50., "Upper bound of track pT for TOF PID"};
  Configurable<double> nSigmaTofMax{"nSigmaTofMax", 5., "Nsigma cut on TOF only"};
  Configurable<double> nSigmaTofCombinedMax{"nSigmaTofCombinedMax", 999., "Nsigma cut on TOF combined with TPC"};
  // topological cuts
  Configurable<std::vector<double>> binsPt{"binsPt", std::vector<double>{hf_cuts_bplus_to_d0_pi::vecBinsPt}, "pT bin limits"};
  Configurable<LabeledArray<double>> cuts{"cuts", {hf_cuts_bplus_to_d0_pi::cuts[0], nBinsPt, nCutVars, labelsPt, labelsCutVar}, "B+ candidate selection per pT bin"};
  // QA switch
  Configurable<bool> activateQA{"activateQA", false, "Flag to enable QA histogram"};

  // check if selectionFlagD (defined in candidateCreatorBplus.cxx) and usePid configurables are in sync
  bool selectionFlagDAndUsePidInSync = true;

  TrackSelectorPID selectorPion;

  using TracksPIDWithSel = soa::Join<aod::BigTracksPIDExtended, aod::TrackSelection>;

  HistogramRegistry registry{"registry"};

  void init(InitContext& initContext)
  {
    if (usePid) {
      selectorPion.setPDG(kPiPlus);
      selectorPion.setRangePtTPC(ptPidTpcMin, ptPidTpcMax);
      selectorPion.setRangeNSigmaTPC(-nSigmaTpcMax, nSigmaTpcMax);
      selectorPion.setRangeNSigmaTPCCondTOF(-nSigmaTpcCombinedMax, nSigmaTpcCombinedMax);
      selectorPion.setRangePtTOF(ptPidTofMin, ptPidTofMax);
      selectorPion.setRangeNSigmaTOF(-nSigmaTofMax, nSigmaTofMax);
      selectorPion.setRangeNSigmaTOFCondTPC(-nSigmaTofCombinedMax, nSigmaTofCombinedMax);
    }

    if (activateQA) {
      constexpr int kNBinsSelections = 1 + SelectionStep::NSelectionSteps;
      std::string labels[kNBinsSelections];
      labels[0] = "No selection";
      labels[1 + SelectionStep::RecoSkims] = "Skims selection";
      labels[1 + SelectionStep::RecoTopol] = "Skims & Topological selections";
      labels[1 + SelectionStep::RecoPID] = "Skims & Topological & PID selections";
      static const AxisSpec axisSelections = {kNBinsSelections, 0.5, kNBinsSelections + 0.5, ""};
      registry.add("hSelections", "Selections;;#it{p}_{T} (GeV/#it{c})", {HistType::kTH2F, {axisSelections, {(std::vector<double>)binsPt, "#it{p}_{T} (GeV/#it{c})"}}});
      for (int iBin = 0; iBin < kNBinsSelections; ++iBin) {
        registry.get<TH2>(HIST("hSelections"))->GetXaxis()->SetBinLabel(iBin + 1, labels[iBin].data());
      }
    }

    int selectionFlagD0 = -1;
    int selectionFlagD0bar = -1;
    auto& workflows = initContext.services().get<RunningWorkflowInfo const>();
    for (DeviceSpec const& device : workflows.devices) {
      if (device.name.compare("hf-candidate-creator-bplus") == 0) {
        for (auto const& option : device.options) {
          if (option.name.compare("selectionFlagD0") == 0) {
            selectionFlagD0 = option.defaultValue.get<int>();
            LOGF(info, "selectionFlagD0 = %d", selectionFlagD0);
          }
          if (option.name.compare("selectionFlagD0bar") == 0) {
            selectionFlagD0bar = option.defaultValue.get<int>();
            LOGF(info, "selectionFlagD0bar = %d", selectionFlagD0bar);
          }
        }
      }
    }
    if ((usePid && !selectionFlagD0) || (usePid && !selectionFlagD0bar)) {
      selectionFlagDAndUsePidInSync = false;
      LOG(warning) << "PID selections required on B+ daughters (usePid=true) but no PID selections on D candidates were required a priori.";
    }
    if ((!usePid && selectionFlagD0) || (!usePid && selectionFlagD0bar)) {
      selectionFlagDAndUsePidInSync = false;
      LOG(warning) << "No PID selections required on Bp daughters (usePid=false) but PID selections on D candidates were required a priori.";
    }
  }

  // Apply topological cuts as defined in SelectorCuts.h; return true if candidate passes all cuts
  /// \param hfCandBplus is the B+ candidate
  /// \param hfCandD0    is prong0 of B+ candidate
  /// \param trackPi     is prong1 of B+ candidate
  /// \return true if candidate passes all selections
  template <typename T1, typename T2, typename T3>
  bool selectionTopol(const T1& hfCandBplus, const T2& hfCandD0, const T3& trackPi)
  {
    auto candpT = hfCandBplus.pt();
    int pTBin = findBin(binsPt, candpT);
    if (pTBin == -1) {
      // Printf("B+ topol selection failed at getpTBin");
      return false;
    }

    // pi pt
    if (trackPi.pt() < cuts->get(pTBin, "pT Pi")) {
      return false;
    }

    // d0(D0)xd0(pi)
    if (hfCandBplus.impactParameterProduct() > cuts->get(pTBin, "Imp. Par. Product")) {
      return false;
    }

    // D0 mass
    if (trackPi.sign() > 0) {
      if (std::abs(invMassD0barToKPi(hfCandD0) - RecoDecay::getMassPDG(pdg::Code::kD0)) > cuts->get(pTBin, "DeltaMD0")) {
        return false;
      }
    }
    if (trackPi.sign() < 0) {
      if (std::abs(invMassD0ToPiK(hfCandD0) - RecoDecay::getMassPDG(pdg::Code::kD0)) > cuts->get(pTBin, "DeltaMD0")) {
        return false;
      }
    }

    // B Decay length
    if (hfCandBplus.decayLength() < cuts->get(pTBin, "B decLen")) {
      return false;
    }

    // B Decay length XY
    if (hfCandBplus.decayLengthXY() < cuts->get(pTBin, "B decLenXY")) {
      return false;
    }

    // B+ CPA cut
    if (hfCandBplus.cpa() < cuts->get(pTBin, "CPA")) {
      return false;
    }

    // if (candpT < ptCandMin || candpT >= ptCandMax) {
    //  Printf("B+ topol selection failed at cand pT check");
    //  return false;
    //  }

    // B+ mass cut
    // if (std::abs(invMassBplusToD0Pi(hfCandBplus) - RecoDecay::getMassPDG(521)) > cuts->get(pTBin, "m")) {
    //  Printf("B+ topol selection failed at mass diff check");
    //   return false;
    //  }

    // d0 of D0 and pi
    if (std::abs(hfCandBplus.impactParameter0()) < cuts->get(pTBin, "d0 D0")) {
      return false;
    }
    if (std::abs(hfCandBplus.impactParameter1()) < cuts->get(pTBin, "d0 Pi")) {
      return false;
    }
    // D0 CPA
    //  if (std::abs(hfCandD0.cpa()) < cuts->get(pTBin, "CPA D0")){
    //   return false;
    // }
    return true;
  }

  /// Apply PID selection
  /// \param pidTrackPi is the PID status of trackPi (prong1 of B+ candidate)
  /// \return true if prong1 of B+ candidate passes all selections
  template <typename T = int>
  bool selectionPID(const T& pidTrackPi)
  {
    if (!acceptPIDNotApplicable && pidTrackPi != TrackSelectorPID::Status::PIDAccepted) {
      return false;
    }
    if (acceptPIDNotApplicable && pidTrackPi == TrackSelectorPID::Status::PIDRejected) {
      return false;
    }

    return true;
  }

  void process(aod::HfCandBplus const& hfCandBs, soa::Join<aod::HfCand2Prong, aod::HfSelD0> const&, TracksPIDWithSel const&)
  {

    for (auto& hfCandB : hfCandBs) { // looping over Bplus candidates

      int statusBplus = 0;
      auto ptCandB = hfCandB.pt();

      // check if flagged as B+ --> D0bar Pi
      if (!(hfCandB.hfflag() & 1 << hf_cand_bplus::DecayType::BplusToD0Pi)) {
        hfSelBplusToD0PiCandidate(statusBplus);
        // Printf("B+ candidate selection failed at hfflag check");
        continue;
      }
      SETBIT(statusBplus, SelectionStep::RecoSkims); // RecoSkims = 0 --> statusBplus = 1
      if (activateQA) {
        registry.fill(HIST("hSelections"), 2 + SelectionStep::RecoSkims, ptCandB);
      }

      // D0 is always index0 and pi is index1 by default
      auto candD0 = hfCandB.prong0_as<soa::Join<aod::HfCand2Prong, aod::HfSelD0>>();
      auto trackPi = hfCandB.prong1_as<TracksPIDWithSel>();

      // topological cuts
      if (!selectionTopol(hfCandB, candD0, trackPi)) {
        hfSelBplusToD0PiCandidate(statusBplus);
        // Printf("B+ candidate selection failed at topology selection");
        continue;
      }
      SETBIT(statusBplus, SelectionStep::RecoTopol); // RecoTopol = 1 --> statusBplus = 3
      if (activateQA) {
        registry.fill(HIST("hSelections"), 2 + SelectionStep::RecoTopol, ptCandB);
      }

      // checking if selectionFlagD0(D0bar) and usePid are in sync
      if (!selectionFlagDAndUsePidInSync) {
        hfSelBplusToD0PiCandidate(statusBplus);
        continue;
      }
      // track-level PID selection
      if (usePid) {
        int pidTrackPi = selectorPion.getStatusTrackPIDTpcAndTof(trackPi);
        if (!selectionPID(pidTrackPi)) {
          // LOGF(info, "B0 candidate selection failed at PID selection");
          hfSelBplusToD0PiCandidate(statusBplus);
          continue;
        }
        SETBIT(statusBplus, SelectionStep::RecoPID); // RecoPID = 2 --> statusBplus = 7
        if (activateQA) {
          registry.fill(HIST("hSelections"), 2 + SelectionStep::RecoPID, ptCandB);
        }
      }

      hfSelBplusToD0PiCandidate(statusBplus);
      // LOGF(info, "Successful B+ candidate selection");
    }
  }
};

WorkflowSpec defineDataProcessing(ConfigContext const& cfgc)
{
  return WorkflowSpec{
    adaptAnalysisTask<HfCandidateSelectorBplusToD0Pi>(cfgc)};
}
