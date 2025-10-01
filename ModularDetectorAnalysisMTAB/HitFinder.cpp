/**
 *  @copyright Copyright 2021 The J-PET Framework Authors. All rights reserved.
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may find a copy of the License in the LICENCE file.
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *  @file HitFinder.cpp
 */

#include "HitFinder.h"
#include "../CommonTools/HitFinderTools.h"
#include <JPetOptionsTools/JPetOptionsTools.h>
#include <JPetWriter/JPetWriter.h>
#include <boost/property_tree/json_parser.hpp>
#include <map>
#include <string>
#include <vector>

using namespace std;
using namespace jpet_options_tools;

HitFinder::HitFinder(const char* name) : JPetUserTask(name) {}

HitFinder::~HitFinder() {}

bool HitFinder::init()
{
  INFO("Hit finding Started");
  fOutputEvents = new JPetTimeWindow("JPetPhysRecoHit");

  // Reading values from the user options if available
  // Getting bools for saving control and calibration histograms
  if (isOptionSet(fParams.getOptions(), kSaveControlHistosParamKey))
  {
    fSaveControlHistos = getOptionAsBool(fParams.getOptions(), kSaveControlHistosParamKey);
  }
  if (isOptionSet(fParams.getOptions(), kSaveCalibHistosParamKey))
  {
    fSaveCalibHistos = getOptionAsBool(fParams.getOptions(), kSaveCalibHistosParamKey);
  }

  if (isOptionSet(fParams.getOptions(), kMinTimeParamKey))
  {
    fMinTime = getOptionAsDouble(fParams.getOptions(), kMinTimeParamKey);
  }
  else
  {
    WARNING(Form("No value of the %s parameter provided by the user. Using default value of %lf.", kMinTimeParamKey.c_str(), fMinTime));
  }
  if (isOptionSet(fParams.getOptions(), kMaxTimeParamKey))
  {
    fMaxTime = getOptionAsDouble(fParams.getOptions(), kMaxTimeParamKey);
  }
  else
  {
    WARNING(Form("No value of the %s parameter provided by the user. Using default value of %lf.", kMaxTimeParamKey.c_str(), fMaxTime));
  }

  // Reading file with effective light velocit and TOF synchronization constants to property tree
  if (isOptionSet(fParams.getOptions(), kConstantsFileParamKey))
  {
    boost::property_tree::read_json(getOptionAsString(fParams.getOptions(), kConstantsFileParamKey), fConstansTree);
  }

  if (isOptionSet(fParams.getOptions(), kMinHitMultiDiffParamKey))
  {
    fMinHitMultiplicity = getOptionAsInt(fParams.getOptions(), kMinHitMultiDiffParamKey);
    INFO(Form("Saving only hits with multiplicity %d or grater", fMinHitMultiplicity));
  }

  // Allowed time difference between signals on A and B sides
  if (isOptionSet(fParams.getOptions(), kABTimeDiffParamKey))
  {
    fABTimeDiff = getOptionAsDouble(fParams.getOptions(), kABTimeDiffParamKey);
  }
  else
  {
    WARNING(Form("No value of the %s parameter provided by the user. Using default value of %lf.", kABTimeDiffParamKey.c_str(), fABTimeDiff));
  }

  // For plotting ToT histograms
  if (isOptionSet(fParams.getOptions(), kToTHistoUpperLimitParamKey))
  {
    fToTHistoUpperLimit = getOptionAsDouble(fParams.getOptions(), kToTHistoUpperLimitParamKey);
  }

  // Control histograms
  if (fSaveControlHistos)
  {
    initialiseHistograms();
  }

  return true;
}

bool HitFinder::exec()
{
  if (auto timeWindow = dynamic_cast<const JPetTimeWindow* const>(fEvent))
  {
    fTWHasTrigger = false;

    auto signalsBySlot = HitFinderTools::getSignalsByScin(timeWindow);

    vector<JPetPhysRecoHit> triggerHitsVec;
    if(signalsBySlot.find(fTriggerScinID) != signalsBySlot.end())
    {
      fTWHasTrigger = true;
      auto triggerMtxSigs = signalsBySlot.at(fTriggerScinID);
      for(auto mtxSig : triggerMtxSigs)
      {
        auto triggerHit = HitFinderTools::createDummyHit(mtxSig, 0.0);
        triggerHitsVec.push_back(triggerHit);
      }
    }

    auto allHits = HitFinderTools::matchAllSignals(signalsBySlot, fABTimeDiff, fConstansTree, getStatistics(), fSaveControlHistos);
    
    if(triggerHitsVec.size() > 0)
    {
      if (fSaveControlHistos)
      {
        getStatistics().fillHistogram("hits_trigger_tslot", triggerHitsVec.size());
        getStatistics().fillHistogram("hits_tslot", allHits.size());

        for(auto hit : allHits)
        {
          double hitTimeMod = hit.getTime() - triggerHitsVec.at(0).getTime();
          getStatistics().fillHistogram("hit_time_mod", hitTimeMod);
          getStatistics().fillHistogram("hit_time_trigger", hit.getTime());
        }
      }
      
      // Sum the hits 
      allHits.insert(allHits.end(), triggerHitsVec.begin(), triggerHitsVec.end());
    }
    else
    {
      if (fSaveControlHistos)
      {
        for(auto hit : allHits)
        {
          getStatistics().fillHistogram("hit_time_notrigger", hit.getTime());
        }
      }
    }

    if (allHits.size() > 0)
    {
      saveHits(allHits);
    }
    
    if (fSaveControlHistos)
    {
      if(fTWHasTrigger)
      {
        getStatistics().fillHistogram("tw_tigger_hit", 1);
      }
      else 
      {
        getStatistics().fillHistogram("tw_tigger_hit", 2);
      }
    }
  }
  else
  {
    return false;
  }
  return true;
}

bool HitFinder::terminate()
{
  INFO("Hit finding ended");
  return true;
}

void HitFinder::saveHits(const std::vector<JPetPhysRecoHit>& hits)
{
  auto sortedHits = hits;
  HitFinderTools::sortByTime(sortedHits);

  for (auto& hit : sortedHits)
  {
    // Checking minimal multiplicity condition
    int multi = hit.getSignalA().getPMSignals().size() + hit.getSignalB().getPMSignals().size();
    if (fMinHitMultiplicity != -1 && multi < fMinHitMultiplicity)
    {
      continue;
    }

    fOutputEvents->add<JPetPhysRecoHit>(hit);
    if (fSaveControlHistos)
    {
      getStatistics().fillHistogram("hit_time", hit.getTime());

      int scinID = hit.getScin().getID();
      getStatistics().fillHistogram("hits_scin", scinID, sortedHits.size());
      getStatistics().fillHistogram("hit_pos", hit.getPosZ(), hit.getPosY(), hit.getPosX());
      getStatistics().fillHistogram("hit_z_pos_scin", scinID, hit.getPosZ());
      getStatistics().fillHistogram("hit_multi", multi);
      getStatistics().fillHistogram("hit_multi_scin", scinID, multi);
      getStatistics().fillHistogram("hit_tdiff_scin", scinID, hit.getTimeDiff());

      if (hit.getToT() != 0.0)
      {
        getStatistics().fillHistogram("hit_tot_scin", scinID, hit.getToT());
        getStatistics().fillHistogram("hit_tot_scin_z_pos", scinID, hit.getToT(), hit.getPosZ());
      }
    }
  }
}

void HitFinder::initialiseHistograms()
{
  auto minScinID = getParamBank().getScins().begin()->first;
  auto maxScinID = getParamBank().getScins().rbegin()->first;

  getStatistics().createHistogramWithAxes(new TH1D("tw_tigger_hit", "Number of time windows with or without trigger Hits", 3, 0.5, 3.5), " ",
                                          "Number of Time Windows");
  vector<pair<unsigned, string>> binLabels = {make_pair(1, "Trigger"), make_pair(2, "No trigger"), make_pair(3, " ")};
  getStatistics().setHistogramBinLabel("tw_tigger_hit", getStatistics().AxisLabel::kXaxis, binLabels);

  getStatistics().createHistogramWithAxes(new TH1D("hit_time", "Hit Time in time window", 200, 1.1 * fMinTime, 1.1 * fMaxTime),
                                          "Hit in Time Slot [ps]", "Number of Time Slots");

  getStatistics().createHistogramWithAxes(new TH1D("hit_time_mod", "Hit Time in time window modified by time of trigger hit time", 200, -1.1 * fMaxTime, 1.1 * fMaxTime),
                                          "Hit in Time Slot [ps]", "Number of Time Slots");

  getStatistics().createHistogramWithAxes(new TH1D("hit_time_trigger", "Hit Time in time window with trigger signal present", 200, 1.1 * fMinTime, 1.1 * fMaxTime),
                                          "Hit in Time Slot [ps]", "Number of Time Slots");
                                          
  getStatistics().createHistogramWithAxes(new TH1D("hit_time_notrigger", "Hit Time in time window without trigger signal", 200, 1.1 * fMinTime, 1.1 * fMaxTime),
                                          "Hit in Time Slot [ps]", "Number of Time Slots");

  getStatistics().createHistogramWithAxes(new TH1D("hits_tslot", "Number of Hits in Time Window", 100, 0.5, 100.5), "Hits in Time Slot",
                                          "Number of Time Slots");

  getStatistics().createHistogramWithAxes(new TH1D("hits_trigger_tslot", "Number of Trigger Hits in Time Window", 100, 0.5, 100.5), "Hits in Time Slot",
                                          "Number of Time Slots");                                          

  getStatistics().createHistogramWithAxes(
      new TH1D("hits_scin", "Number of Hits per Scintillators", maxScinID - minScinID + 1, minScinID - 0.5, maxScinID + 0.5), "Scin ID",
      "Number of Hits");

  getStatistics().createHistogramWithAxes(new TH3D("hit_pos", "Hit Position", 101, -50.5, 50.5, 101, -50.5, 50.5, 101, -50.5, 50.5), "Z [cm]",
                                          "Y [cm]", "X [cm]");

  getStatistics().createHistogramWithAxes(new TH2D("hit_z_pos_scin", "Z-axis position of hits in Scintillators", maxScinID - minScinID + 1,
                                                   minScinID - 0.5, maxScinID + 0.5, 121, -30.5, 30.5),
                                          "Scintillator ID", "Hit z-pos [cm]");

  getStatistics().createHistogramWithAxes(new TH1D("hit_multi", "Number of signals from SiPMs in created hit", 12, -0.5, 11.5), "Number of Signals",
                                          "Number of Hits");

  getStatistics().createHistogramWithAxes(new TH2D("hit_multi_scin", "Number of signals from SiPMs in created hit per Scin",
                                                   maxScinID - minScinID + 1, minScinID - 0.5, maxScinID + 0.5, 12, -0.5, 11.5),
                                          "Scintillator ID", "Signal multiplicity [ps]");

  // Time diff and ToT per scin
  getStatistics().createHistogramWithAxes(new TH2D("hit_tdiff_scin", "Hit Time Difference per Scintillator ID", maxScinID - minScinID + 1,
                                                   minScinID - 0.5, maxScinID + 0.5, 201, -1.1 * fABTimeDiff, 1.1 * fABTimeDiff),
                                          "Scintillator ID", "A-B time difference [ps]");

  getStatistics().createHistogramWithAxes(new TH2D("hit_tot_scin", "Hit ToT divided by multiplicity, all hits", maxScinID - minScinID + 1,
                                                   minScinID - 0.5, maxScinID + 0.5, 200, 0.0, 1.2 * fToTHistoUpperLimit),
                                          "Scintillator ID", "Time over Threshold [ps]");

  getStatistics().createHistogramWithAxes(new TH3D("hit_tot_scin_z_pos", "Hit ToT divided by multiplicity per scin vs z-axis",
                                                   maxScinID - minScinID + 1, minScinID - 0.5, maxScinID + 0.5, 200, 0.0, 1.2 * fToTHistoUpperLimit,
                                                   121, -30.5, 30.5),
                                          "Scintillator ID", "Time over Threshold [ps]", "z [cm]");

  // Unused sigals stats
  getStatistics().createHistogramWithAxes(
      new TH1D("remain_signals_scin", "Number of Unused Signals in Scintillator", maxScinID - minScinID + 1, minScinID - 0.5, maxScinID + 0.5),
      "Scintillator ID", "Number of Unused Signals in Scintillator");

  getStatistics().createHistogramWithAxes(
      new TH1D("remain_signals_tdiff", "Time Diff of an unused signal and the consecutive one", 200, fABTimeDiff, 5.0 * fABTimeDiff),
      "Time difference [ps]", "Number of Signals");
}
