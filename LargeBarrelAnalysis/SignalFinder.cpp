/**
 *  @copyright Copyright 2018 The J-PET Framework Authors. All rights reserved.
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
 *  @file SignalFinder.cpp
 */

using namespace std;

#include <JPetOptionsTools/JPetOptionsTools.h>
#include <JPetTimeWindow/JPetTimeWindow.h>
#include <JPetWriter/JPetWriter.h>
#include "SignalFinderTools.h"
#include "SignalFinder.h"
#include <utility>
#include <string>
#include <vector>

using namespace jpet_options_tools;

/**
 * Constructor
 */
SignalFinder::SignalFinder(const char* name): JPetUserTask(name) {}

/**
 * Destructor
 */
SignalFinder::~SignalFinder() {}

/**
 * Init Signal Finder
 */
bool SignalFinder::init()
{
  INFO("Signal finding started.");
  fOutputEvents = new JPetTimeWindow("JPetRawSignal");

  // Reading values from the user options if available
  // Time window parameter for leading edge
  if (isOptionSet(fParams.getOptions(), kEdgeMaxTimeParamKey)) {
    fSigChEdgeMaxTime = getOptionAsFloat(fParams.getOptions(), kEdgeMaxTimeParamKey);
  } else {
    WARNING(Form("No value of the %s parameter provided by the user. Using default value of %lf.",
      kEdgeMaxTimeParamKey.c_str(), fSigChEdgeMaxTime)
    );
  }

  // Time window parameter for leading-trailing comparison
  if (isOptionSet(fParams.getOptions(), kLeadTrailMaxTimeParamKey)) {
    fSigChLeadTrailMaxTime = getOptionAsFloat(fParams.getOptions(), kLeadTrailMaxTimeParamKey);
  } else {
    WARNING(Form("No value of the %s parameter provided by the user. Using default value of %lf.",
      kLeadTrailMaxTimeParamKey.c_str(), fSigChLeadTrailMaxTime));
  }

  // Get bool for using bad Signal Channels
  if (isOptionSet(fParams.getOptions(), kUseCorruptedSigChParamKey)) {
    fUseCorruptedSigCh = getOptionAsBool(fParams.getOptions(), kUseCorruptedSigChParamKey);
    if(fUseCorruptedSigCh){
      WARNING("Signal Finder is using Corrupted Signal Channels, as set by the user");
    } else{
      WARNING("Signal Finder is NOT using Corrupted Signal Channels, as set by the user");
    }
  } else {
    WARNING("Signal Finder is not using Corrupted Signal Channels (default option)");
  }

  // Getting bool for saving histograms
  if (isOptionSet(fParams.getOptions(), kSaveControlHistosParamKey)) {
    fSaveControlHistos = getOptionAsBool(fParams.getOptions(), kSaveControlHistosParamKey);
  }

  // Creating control histograms
  if(fSaveControlHistos) { initialiseHistograms(); }
  return true;
}

/**
 * Execute Signal Finder - sorting by PM, removing multiple edges, building signals
 */
bool SignalFinder::exec()
{
  // Getting the data from event in an apropriate format
  if(auto timeWindow = dynamic_cast<const JPetTimeWindow* const>(fEvent)) {
    // Distribute signal channels by PM IDs and filter out Corrupted SigChs if requested
    auto sigChByPM = SignalFinderTools::getSigChByPM(timeWindow, getParamBank(), fUseCorruptedSigCh);
    // Building signals
    auto allSignals = SignalFinderTools::buildAllSignals(
      sigChByPM, kNumOfThresholds, fSigChEdgeMaxTime, fSigChLeadTrailMaxTime,
      getStatistics(), fSaveControlHistos
    );
    // Saving method invocation
    saveRawSignals(allSignals);
  } else { return false; }
  return true;
}

/**
 * Terminate Signal Finder
 */
bool SignalFinder::terminate()
{
  INFO("Signal finding ended.");
  return true;
}

/**
 * Saving method
 */
void SignalFinder::saveRawSignals(const vector<JPetRawSignal>& rawSigVec)
{
  for (auto & rawSig : rawSigVec) { fOutputEvents->add<JPetRawSignal>(rawSig); }
}

/**
 * Init histograms
 */
void SignalFinder::initialiseHistograms(){

  getStatistics().createHistogram(new TH1F(
    "L_time_diff",
    "Time Difference between leading Signal Channels in found Raw Signals",
    200, 0.0, fSigChEdgeMaxTime
  ));
  getStatistics().getHisto1D("L_time_diff")
    ->GetXaxis()->SetTitle("Time difference [ps]");
  getStatistics().getHisto1D("L_time_diff")
    ->GetYaxis()->SetTitle("Number of Signal Channels Pairs");

  getStatistics().createHistogram(new TH1F(
    "LT_time_diff",
    "Time Difference between leading and trailing Signal Channels in found signals",
    200, 0.0, fSigChLeadTrailMaxTime
  ));
  getStatistics().getHisto1D("LT_time_diff")
    ->GetXaxis()->SetTitle("Time difference [ps]");
  getStatistics().getHisto1D("LT_time_diff")
    ->GetYaxis()->SetTitle("Number of Signal Channels Pairs");

  getStatistics().createHistogram(new TH1F(
    "good_v_bad_raw_sigs", "Number of good and corrupted signals created",
    2, 0.5, 2.5
  ));
  getStatistics().getHisto1D("good_v_bad_raw_sigs")->GetXaxis()->SetBinLabel(1,"GOOD");
  getStatistics().getHisto1D("good_v_bad_raw_sigs")->GetXaxis()->SetBinLabel(2,"CORRUPTED");
  getStatistics().getHisto1D("good_v_bad_raw_sigs")->GetYaxis()->SetTitle("Number of Raw Signals");
}
