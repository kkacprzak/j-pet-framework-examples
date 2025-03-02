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
 *  @file TimeWindowCreator.cpp
 */

#include "TimeWindowCreator.h"
#include "../CommonTools/TimeWindowCreatorTools.h"
#include <JPetOptionsTools/JPetOptionsTools.h>
#include <JPetTaskIO/JPetInputHandlerHLD.h>
#include <JPetWriter/JPetWriter.h>
#include <Signals/JPetChannelSignal/JPetChannelSignal.h>

#include <boost/property_tree/json_parser.hpp>

#include <iostream>
#include <unpacker_types.hpp>
#include <utility>

using namespace jpet_options_tools;
using namespace std;
namespace pt = boost::property_tree;

TimeWindowCreator::TimeWindowCreator(const char* name) : JPetUserTask(name) {}

TimeWindowCreator::~TimeWindowCreator() {}

bool TimeWindowCreator::init()
{
  INFO("TimeSlot Creation Started");
  fOutputEvents = new JPetTimeWindow("JPetChannelSignal");

  // Reading values from the user options if available
  // Min allowed signal time
  if (isOptionSet(fParams.getOptions(), kMinTimeParamKey))
  {
    fMinTime = getOptionAsDouble(fParams.getOptions(), kMinTimeParamKey);
  }
  else
  {
    WARNING(Form("No value of the %s parameter provided by the user. Using default value of %lf.", kMinTimeParamKey.c_str(), fMinTime));
  }
  // Max allowed signal time
  if (isOptionSet(fParams.getOptions(), kMaxTimeParamKey))
  {
    fMaxTime = getOptionAsDouble(fParams.getOptions(), kMaxTimeParamKey);
  }
  else
  {
    WARNING(Form("No value of the %s parameter provided by the user. Using default value of %lf.", kMaxTimeParamKey.c_str(), fMaxTime));
  }

  // Getting the calibration file from user options
  if (isOptionSet(fParams.getOptions(), kConstantsFileParamKey))
  {
    pt::read_json(getOptionAsString(fParams.getOptions(), kConstantsFileParamKey), fConstansTree);
  }

  // Getting bool for saving histograms
  if (isOptionSet(fParams.getOptions(), kSaveControlHistosParamKey))
  {
    fSaveControlHistos = getOptionAsBool(fParams.getOptions(), kSaveControlHistosParamKey);
  }

  // build a lookup table of channel offsets
  for (auto& dm : getParamBank().getDataModules())
  {
    fChannelOffsets[dm.second->getTBRNetAddress()] = dm.second->getChannelsOffset();
  }

  // Control histograms
  if (fSaveControlHistos)
  {
    initialiseHistograms();
  }
  return true;
}

bool TimeWindowCreator::exec()
{
  if (auto event = dynamic_cast<JPetHLDdata* const>(fEvent))
  {
    vector<JPetChannelSignal> allChannelSignals;
    unordered_map<int, vector<JPetChannelSignal>> singleChannelSignals;

    for (auto& endp_data : event->fOriginalData)
    {
      unsigned int address = endp_data.first;
      if (fChannelOffsets.count(address) == 0)
      {
        continue;
      }
      unsigned int channel_offset = fChannelOffsets.at(address);

      std::vector<unpacker::hit_t>& data = endp_data.second;

      for (auto& hit : data)
      {
        int channelNumber = channel_offset + hit.channel_id;

        // Skip trigger signals - every 65th
        if (channelNumber % 65 == 0)
        {
          continue;
        }

        // Skip if the channel number is absent in the configuration
        if (getParamBank().getChannels().count(channelNumber) == 0)
        {
          if (fSaveControlHistos)
          {
            getStatistics().fillHistogram("wrong_channel", channelNumber);
          }
          continue;
        }

        auto& channel = getParamBank().getChannel(channelNumber);
        double offset = fConstansTree.get("channel_offests." + to_string(channel.getID()), 0.0);

        double time = hit.time / 1000.;

        time = time - (fMaxTime - fMinTime);
        time *= -1.;

        if (time < fMinTime || time > fMaxTime)
        {
          continue;
        }

        auto sigCh = TimeWindowCreatorTools::generateChannelSignal(
            time, channel, hit.is_falling_edge == 0 ? JPetChannelSignal::Leading : JPetChannelSignal::Trailing, offset);
        singleChannelSignals[channel.getID()].push_back(sigCh);
      }
    }

    for (auto& chSigs : singleChannelSignals)
    {
      TimeWindowCreatorTools::flagChannelSignals(chSigs.second, getStatistics(), fSaveControlHistos);
      // Sort Signal Channels in time
      TimeWindowCreatorTools::sortByTime(chSigs.second);
      allChannelSignals.insert(allChannelSignals.end(), chSigs.second.begin(), chSigs.second.end());
    }
    // Save result
    saveChannelSignals(allChannelSignals);
  }
  else
  {
    return false;
  }
  return true;
}

bool TimeWindowCreator::terminate()
{
  INFO("TimeSlot Creation Ended");
  return true;
}

void TimeWindowCreator::saveChannelSignals(const vector<JPetChannelSignal>& channelSigVec)
{
  if (fSaveControlHistos)
  {
    getStatistics().fillHistogram("chsig_tslot", channelSigVec.size());
  }

  for (auto& channelSig : channelSigVec)
  {
    fOutputEvents->add<JPetChannelSignal>(channelSig);
    if (fSaveControlHistos)
    {
      getStatistics().fillHistogram("pm_occ", channelSig.getChannel().getPM().getID());
      getStatistics().fillHistogram(Form("pm_occ_thr%d", channelSig.getChannel().getThresholdNumber()), channelSig.getChannel().getPM().getID());
    }
  }
}

void TimeWindowCreator::initialiseHistograms()
{
  getStatistics().createHistogramWithAxes(new TH1D("chsig_tslot", "Signal Channels Per Time Slot", 50, 0.5, 50.5), "Channels Signal in Time Slot",
                                          "Number of Time Slots");

  // Channels and PMs IDs from Param Bank
  auto minChannelID = getParamBank().getChannels().begin()->first;
  auto maxChannelID = getParamBank().getChannels().rbegin()->first;

  auto minPMID = getParamBank().getPMs().begin()->first;
  auto maxPMID = getParamBank().getPMs().rbegin()->first;

  // Wrong configuration
  getStatistics().createHistogramWithAxes(new TH1D("wrong_channel", "Channel IDs not found in the json configuration",
                                                   maxChannelID - minChannelID + 1, minChannelID - 0.5, maxChannelID + 0.5),
                                          "Channel ID", "Number of Channel Signals");

  getStatistics().createHistogramWithAxes(new TH1D("pm_occ", "Channels Signals per PM", maxPMID - minPMID + 1, minPMID - 0.5, maxPMID + 0.5), "PM ID",
                                          "Number of Channel Signals");

  for (int i = 1; i <= kNumOfThresholds; i++)
  {
    getStatistics().createHistogramWithAxes(
        new TH1D(Form("pm_occ_thr%d", i), Form("Channels Signals per PM on THR %d", i), maxPMID - minPMID + 1, minPMID - 0.5, maxPMID + 0.5), "PM ID",
        "Number of Channel Signals");
  }

  getStatistics().createHistogramWithAxes(new TH1D("good_vs_bad_sigch", "Number of good and corrupted SigChs created", 3, 0.5, 3.5), "Quality",
                                          "Number of SigChs");
  std::vector<std::pair<unsigned, std::string>> binLabels;
  binLabels.push_back(std::make_pair(1, "GOOD"));
  binLabels.push_back(std::make_pair(2, "CORRUPTED"));
  binLabels.push_back(std::make_pair(3, "UNKNOWN"));
  getStatistics().setHistogramBinLabel("good_vs_bad_sigch", getStatistics().AxisLabel::kXaxis, binLabels);

  getStatistics().createHistogramWithAxes(new TH1D("LT_time_diff", "LT time diff", 200, -250.0, 999750.0), "Time Diff [ps]", "Number of LL pairs");
  getStatistics().createHistogramWithAxes(new TH1D("LL_per_PM", "Number of LL found on PMs", 385, 0.5, 385.5), "PM ID", "Number of LL pairs");
  getStatistics().createHistogramWithAxes(new TH1D("LL_per_THR", "Number of found LL on Thresolds", 4, 0.5, 4.5), "THR Number", "Number of LL pairs");
  getStatistics().createHistogramWithAxes(new TH1D("LL_time_diff", "Time diff of LL pairs", 200, -750.0, 299250.0), "Time Diff [ps]",
                                          "Number of LL pairs");
  getStatistics().createHistogramWithAxes(new TH1D("TT_per_PM", "Number of TT found on PMs", 385, 0.5, 385.5), "PM ID", "Number of TT pairs");
  getStatistics().createHistogramWithAxes(new TH1D("TT_per_THR", "Number of found TT on Thresolds", 4, 0.5, 4.5), "THR Number", "Number of TT pairs");
  getStatistics().createHistogramWithAxes(new TH1D("TT_time_diff", "Time diff of TT pairs", 200, -750.0, 299250.0), "Time Diff [ps]",
                                          "Number of TT pairs");
}
