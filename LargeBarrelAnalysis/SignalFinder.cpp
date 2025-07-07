/**
 *  @copyright Copyright 2020 The J-PET Framework Authors. All rights reserved.
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

#include "SignalFinder.h"
#include <JPetOptionsTools/JPetOptionsTools.h>
#include <JPetTimeWindow/JPetTimeWindow.h>
#include <JPetWriter/JPetWriter.h>
#include <TRandom.h>
#include <string>
#include <utility>
#include <vector>
#include <boost/property_tree/json_parser.hpp>

using namespace jpet_options_tools;

SignalFinder::SignalFinder(const char* name) : JPetUserTask(name) {}

SignalFinder::~SignalFinder() {}

bool SignalFinder::init()
{
  INFO("Signal finding started.");
  fOutputEvents = new JPetTimeWindow("JPetPMSignal");

  // Reading values from the user options if available
  // Time window parameter for leading edge
  if (isOptionSet(fParams.getOptions(), kEdgeMaxTimeParamKey))
  {
    fEdgeMaxTime = getOptionAsDouble(fParams.getOptions(), kEdgeMaxTimeParamKey);
  }
  else
  {
    WARNING(Form("No value of the %s parameter provided by the user. Using default value of %lf.", kEdgeMaxTimeParamKey.c_str(), fEdgeMaxTime));
  }

  // Time window parameter for leading-trailing comparison
  if (isOptionSet(fParams.getOptions(), kLeadTrailMaxTimeParamKey))
  {
    fLeadTrailMaxTime = getOptionAsDouble(fParams.getOptions(), kLeadTrailMaxTimeParamKey);
  }
  else
  {
    WARNING(
        Form("No value of the %s parameter provided by the user. Using default value of %lf.", kLeadTrailMaxTimeParamKey.c_str(), fLeadTrailMaxTime));
  }

  // For plotting ToT histograms
  if (isOptionSet(fParams.getOptions(), kToTHistoUpperLimitParamKey))
  {
    fToTHistoUpperLimit = getOptionAsDouble(fParams.getOptions(), kToTHistoUpperLimitParamKey);
  }

  if (isOptionSet(fParams.getOptions(), kToTCalculationTypeParamKey))
  {
    if (getOptionAsString(fParams.getOptions(), kToTCalculationTypeParamKey) == "simple")
    {
      fToTCalcType = SignalFinderTools::kSimplified;
    }
    else if (getOptionAsString(fParams.getOptions(), kToTCalculationTypeParamKey) == "rectangular")
    {
      fToTCalcType = SignalFinderTools::kThresholdRectangular;
    }
    else if (getOptionAsString(fParams.getOptions(), kToTCalculationTypeParamKey) == "trapeze")
    {
      fToTCalcType = SignalFinderTools::kThresholdTrapeze;
    }
  }
  else
  {
    WARNING("Unrecognized name for method of calculating ToT provided: use simple, rectangular of trapeze. Using default simplified method.");
  }

  // Get bool for using corrupted Channel Signals
  if (isOptionSet(fParams.getOptions(), kUseCorruptedSigChParamKey))
  {
    fUseCorruptedChannelSignals = getOptionAsBool(fParams.getOptions(), kUseCorruptedSigChParamKey);
    if (fUseCorruptedChannelSignals)
    {
      INFO("Signal Finder is using Corrupted Signal Channels, as set by the user");
    }
    else
    {
      INFO("Signal Finder is NOT using Corrupted Signal Channels, as set by the user");
    }
  }
  else
  {
    INFO("Signal Finder is not using Corrupted Signal Channels (default option)");
  }

  // Option for requiring all thresholds to be recinstructed in the signal
  if (isOptionSet(fParams.getOptions(), kRequireAllThresholdsParamKey))
  {
    fRequireAllThresholds = getOptionAsBool(fParams.getOptions(), kRequireAllThresholdsParamKey);
    if (fRequireAllThresholds)
    {
      INFO("Saving only signals with all the threshlds reconstructed correctly.");
    }
  }

  // Reference detector photomultiplier identifier
  if (isOptionSet(fParams.getOptions(), kRefPMIDParamKey))
  {
    fRefPMID = getOptionAsInt(fParams.getOptions(), kRefPMIDParamKey);
    INFO("Signal Finder is ignoring Corrupted Signal Channels from the Refference detector as specified by the user (photomultiplier number + "
         "std::to_string(fRefPMID)");
  }
  else
  {
    INFO(
        "Signal Finder is ignoring Corrupted Signal Channels from the Refference detector(default photomultiplier number + std::to_string(fRefPMID)");
  }

  // Getting bool for saving histograms
  if (isOptionSet(fParams.getOptions(), kSaveControlHistosParamKey))
  {
    fSaveControlHistos = getOptionAsBool(fParams.getOptions(), kSaveControlHistosParamKey);
  }

  // Reading the calibration
  if (isOptionSet(fParams.getOptions(), kConstantsFileParamKey))
  {
    boost::property_tree::read_json(getOptionAsString(fParams.getOptions(), kConstantsFileParamKey), fConstansTree);
  }

  // Check if the user requested ordering of thresholds by value
  if (isOptionSet(fParams.getOptions(), kOrderThresholdsByValueKey))
  {
    fOrderThresholdsByValue = getOptionAsBool(fParams.getOptions(), kOrderThresholdsByValueKey);
  }
  if (fOrderThresholdsByValue)
  {
    INFO("Threshold reordering was requested. Thresholds will be ordered by their values according to provided detector setup file.");
    fThresholdOrderings = SignalFinderTools::findThresholdOrder(getParamBank());
  }

  // Creating control histograms
  if (fSaveControlHistos)
  {
    initialiseHistograms();
  }
  return true;
}

bool SignalFinder::exec()
{
  // Getting the data from event in an apropriate format
  if (auto timeWindow = dynamic_cast<const JPetTimeWindow* const>(fEvent))
  {
    // Distribute signal channels by PM IDs and filter out Corrupted SigChs if requested
    auto& chSigsPMMap = SignalFinderTools::getChannelSignalsByPM(timeWindow, fUseCorruptedChannelSignals, fRefPMID);

    // Building signals
    auto allSignals = SignalFinderTools::buildAllSignals(chSigsPMMap, fEdgeMaxTime, fLeadTrailMaxTime, kNumOfThresholds, getStatistics(),
                                                         fSaveControlHistos, fToTCalcType, fConstansTree, fThresholdOrderings);

    savePMSignals(allSignals);
  }
  else
  {
    return false;
  }
  return true;
}

bool SignalFinder::terminate()
{
  INFO("Signal finding ended.");
  return true;
}

void SignalFinder::savePMSignals(const vector<JPetPMSignal>& pmSigVec)
{
  if (pmSigVec.size() > 0 && fSaveControlHistos)
  {
    getStatistics().fillHistogram("pmsig_tslot", pmSigVec.size());
  }

  for (auto& pmSig : pmSigVec)
  {
    // Skip saving this signal if there is a requirement for threshld number
    if (fRequireAllThresholds && pmSig.getLeadTrailPairs().size() != kNumOfThresholds)
    {
      continue;
    }

    fOutputEvents->add<JPetPMSignal>(pmSig);
    if (fSaveControlHistos)
    {
      getStatistics().fillHistogram("pmsig_multi", pmSig.getLeadTrailPairs().size());
      getStatistics().fillHistogram("pmsig_pm_id", pmSig.getPM().getID());
      if (pmSig.getToT() != 0.0)
      {
        getStatistics().fillHistogram("pmsig_tot_pm_id", pmSig.getPM().getID(), pmSig.getToT());
      }
    }
  }
}

void SignalFinder::initialiseHistograms()
{
  auto minPMID = getParamBank().getPMs().begin()->first;
  auto maxPMID = getParamBank().getPMs().rbegin()->first;

  // Occupancies and multiplicities
  getStatistics().createHistogramWithAxes(new TH1D("pmsig_pm_id", "PM Signals per PMT ID", maxPMID - minPMID + 1, minPMID - 0.5, maxPMID + 0.5),
                                          "PMT ID", "Number of PM Signals");

  getStatistics().createHistogramWithAxes(new TH1D("pmsig_multi", "PM Signal Multiplicity", 6, 0.5, 6.5), "Total number of ChSigs in PMSig",
                                          "Number of Signal Channels");

  getStatistics().createHistogramWithAxes(new TH1D("pmsig_tslot", "Number of PM Signals in Time Window", 100, 0.5, 100.5),
                                          "Number of PM Signal in Time Window", "Number of Time Windows");

  // ToT of signals
  getStatistics().createHistogramWithAxes(new TH2D("pmsig_tot_pm_id", "PMT Signal Time over Threshold per PMT ID", maxPMID - minPMID + 1,
                                                   minPMID - 0.5, maxPMID + 0.5, 200, 0.0, fToTHistoUpperLimit),
                                          "PMT ID", "ToT [ps]");

  getStatistics().createHistogramWithAxes(new TH1D("reco_flags_pmsig", "Number of good and corrupted Channel Sigals created", 4, 0.5, 4.5), " ",
                                          "Number of Channel Signals");
  vector<pair<unsigned, string>> binLabels1 = {make_pair(1, "GOOD"), make_pair(2, "CORRUPTED"), make_pair(3, "UNKNOWN"), make_pair(4, " ")};
  getStatistics().setHistogramBinLabel("reco_flags_pmsig", getStatistics().AxisLabel::kXaxis, binLabels1);

  vector<pair<unsigned, string>> binLabels;
  binLabels.push_back(std::make_pair(1, "THR 1 Lead"));
  binLabels.push_back(std::make_pair(2, "THR 1 Trail"));
  binLabels.push_back(std::make_pair(3, "THR 2 Lead"));
  binLabels.push_back(std::make_pair(4, "THR 2 Trail"));
  binLabels.push_back(std::make_pair(5, "THR 3 Lead"));
  binLabels.push_back(std::make_pair(6, "THR 3 Trail"));
  binLabels.push_back(std::make_pair(7, "THR 4 Lead"));
  binLabels.push_back(std::make_pair(8, "THR 4 Trail"));
  binLabels.push_back(std::make_pair(9, " "));
  getStatistics().createHistogramWithAxes(new TH1D("unused_chsig_thr", "Unused Channel Signals per THR (downscaled)", 9, 0.5, 9.5), " ",
                                          "Number of Channel Signals");
  getStatistics().setHistogramBinLabel("unused_chsig_thr", getStatistics().AxisLabel::kXaxis, binLabels);

  getStatistics().createHistogramWithAxes(
      new TH1D("unused_chsig_pm", "Unused Signal Channels per PMT", maxPMID - minPMID + 1, minPMID - 0.5, maxPMID + 0.5), "PMT ID",
      "Number of Channel Signals");

  // Threshold time differences
  getStatistics().createHistogramWithAxes(new TH1D("lead_thr1_thr2_diff",
                                                   "Time Difference between leading Signal Channels THR1 and THR2 in found signals", 200,
                                                   -fEdgeMaxTime, fEdgeMaxTime),
                                          "time diff [ps]", "Number of Signal Channels Pairs");
  getStatistics().createHistogramWithAxes(new TH1D("lead_thr1_thr3_diff",
                                                   "Time Difference between leading Signal Channels THR1 and THR3 in found signals", 200,
                                                   -fEdgeMaxTime, fEdgeMaxTime),
                                          "time diff [ps]", "Number of Signal Channels Pairs");
  getStatistics().createHistogramWithAxes(new TH1D("lead_thr1_thr4_diff",
                                                   "Time Difference between leading Signal Channels THR1 and THR4 in found signals", 200,
                                                   -fEdgeMaxTime, fEdgeMaxTime),
                                          "time diff [ps]", "Number of Signal Channels Pairs");
  getStatistics().createHistogramWithAxes(new TH1D("lead_trail_thr1_diff",
                                                   "Time Difference between leading and trailing Signal Channels THR1 in found signals", 200, 0.0,
                                                   fLeadTrailMaxTime),
                                          "time diff [ps]", "Number of Signal Channels Pairs");
  getStatistics().createHistogramWithAxes(new TH1D("lead_trail_thr2_diff",
                                                   "Time Difference between leading and trailing Signal Channels THR2 in found signals", 200, 0.0,
                                                   fLeadTrailMaxTime),
                                          "time diff [ps]", "Number of Signal Channels Pairs");
  getStatistics().createHistogramWithAxes(new TH1D("lead_trail_thr3_diff",
                                                   "Time Difference between leading and trailing Signal Channels THR3 in found signals", 200, 0.0,
                                                   fLeadTrailMaxTime),
                                          "time diff [ps]", "Number of Signal Channels Pairs");
  getStatistics().createHistogramWithAxes(new TH1D("lead_trail_thr4_diff",
                                                   "Time Difference between leading and trailing Signal Channels THR4 in found signals", 200, 0.0,
                                                   fLeadTrailMaxTime),
                                          "time diff [ps]", "Number of Signal Channels Pairs");
  // Per PM for calibration
  getStatistics().createHistogramWithAxes(new TH2D("lead_thr1_thr2_diff_pm",
                                                   "Time Difference between leading Signal Channels THR1 and THR2 in found signals per PM",
                                                   maxPMID - minPMID + 1, minPMID - 0.5, maxPMID + 0.5, 200, -fEdgeMaxTime, fEdgeMaxTime),
                                          "PM ID", "time diff [ps]");

  getStatistics().createHistogramWithAxes(new TH2D("lead_thr1_thr3_diff_pm",
                                                   "Time Difference between leading Signal Channels THR1 and THR3 in found signals per PM",
                                                   maxPMID - minPMID + 1, minPMID - 0.5, maxPMID + 0.5, 200, -fEdgeMaxTime, fEdgeMaxTime),
                                          "PM ID", "time diff [ps]");

  getStatistics().createHistogramWithAxes(new TH2D("lead_thr1_thr4_diff_pm",
                                                   "Time Difference between leading Signal Channels THR1 and THR4 in found signals per PM",
                                                   maxPMID - minPMID + 1, minPMID - 0.5, maxPMID + 0.5, 200, -fEdgeMaxTime, fEdgeMaxTime),
                                          "PM ID", "time diff [ps]");

  getStatistics().createHistogramWithAxes(new TH2D("lead_trail_thr1_diff_pm",
                                                   "Time Difference between leading and trailing Signal Channels THR1 in found signals per PM",
                                                   maxPMID - minPMID + 1, minPMID - 0.5, maxPMID + 0.5, 200, 0.0, fLeadTrailMaxTime),
                                          "PM ID", "time diff [ps]");

  getStatistics().createHistogramWithAxes(new TH2D("lead_trail_thr2_diff_pm",
                                                   "Time Difference between leading and trailing Signal Channels THR2 in found signals per PM",
                                                   maxPMID - minPMID + 1, minPMID - 0.5, maxPMID + 0.5, 200, 0.0, fLeadTrailMaxTime),
                                          "PM ID", "time diff [ps]");

  getStatistics().createHistogramWithAxes(new TH2D("lead_trail_thr3_diff_pm",
                                                   "Time Difference between leading and trailing Signal Channels THR3 in found signals per PM",
                                                   maxPMID - minPMID + 1, minPMID - 0.5, maxPMID + 0.5, 200, 0.0, fLeadTrailMaxTime),
                                          "PM ID", "time diff [ps]");

  getStatistics().createHistogramWithAxes(new TH2D("lead_trail_thr4_diff_pm",
                                                   "Time Difference between leading and trailing Signal Channels THR4 in found signals per PM",
                                                   maxPMID - minPMID + 1, minPMID - 0.5, maxPMID + 0.5, 200, 0.0, fLeadTrailMaxTime),
                                          "PM ID", "time diff [ps]");
}
