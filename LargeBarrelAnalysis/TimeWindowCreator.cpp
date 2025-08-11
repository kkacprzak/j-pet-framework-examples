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
#include "Unpacker2/EventIII.h"
#include "Unpacker2/TDCChannel.h"
#include <JPetOptionsTools/JPetOptionsTools.h>
// #include <JPetTaskIO/JPetInputHandlerHLD.h>
#include <JPetWriter/JPetWriter.h>
#include <Signals/JPetChannelSignal/JPetChannelSignal.h>

#include <boost/property_tree/json_parser.hpp>

#include <iostream>
// #include <unpacker_types.hpp>
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
    // std::cout << "Address: " << std::hex << dm.second->getTBRNetAddress() << " Offset: " << dm.second->getChannelsOffset() << std::endl;
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
  // if (auto event = dynamic_cast<JPetHLDdata* const>(fEvent))
  if (auto event = dynamic_cast<EventIII* const>(fEvent))
  {
    int kTDCChannels = event->GetTotalNTDCChannels();
    auto tdcChannels = event->GetTDCChannelsArray();
    for (int i = 0; i < kTDCChannels; ++i)
    {
      auto tdcChannel = dynamic_cast<TDCChannel* const>(tdcChannels->At(i));
      auto channelID = tdcChannel->GetChannel();

      // Skip trigger signals from TRB - every 65th
      if (channelID % 65 == 0)
      {
        continue;
      }

      // Check if channel exists in database from loaded local file
      if (getParamBank().getChannels().count(channelID) == 0)
      {
        if (fSaveControlHistos)
        {
          getStatistics().fillHistogram("wrong_channel", channelID);
        }
        continue;
      }
      // Get channel for corresponding number
      auto& channel = getParamBank().getChannel(channelID);
      double synchroOffset =
          fConstansTree.get("pm_thr_offsets." + to_string(channel.getPM().getID()) + "." + to_string(channel.getThresholdNumber()), 0.0);

      // Building Channel Signals for this TDC Channel
      auto allChSigs = TimeWindowCreatorTools::buildChannelSignals(tdcChannel, channel, fMaxTime, fMinTime, synchroOffset);

      // Sort Signal Channels in time
      TimeWindowCreatorTools::sortByTime(allChSigs);

      // Flag with Good or Corrupted
      TimeWindowCreatorTools::flagChannelSignals(allChSigs, getStatistics(), fSaveControlHistos);

      // Save result
      saveChannelSignals(allChSigs);
    }

    ///////////////////////////////////////////////////////////

    // vector<JPetChannelSignal> allChannelSignals;
    // unordered_map<int, vector<JPetChannelSignal>> singleChannelSignals;

    // for (auto& endp_data : event->fOriginalData)
    // {
    //   unsigned int address = endp_data.first;
    //
    //   if (fChannelOffsets.count(address) == 0)
    //   {
    //     continue;
    //   }
    //   unsigned int channel_offset = fChannelOffsets.at(address);
    //
    //   std::vector<unpacker::hit_t>& data = endp_data.second;
    //
    //   for (auto& hit : data)
    //   {
    //     int channelNumber = channel_offset + hit.channel_id;
    //
    //     // Skip trigger signals - every 65th
    //     if (channelNumber % 65 == 0)
    //     {
    //       continue;
    //     }
    //
    //     // Skip if the channel number is absent in the configuration
    //     if (getParamBank().getChannels().count(channelNumber) == 0)
    //     {
    //       if (fSaveControlHistos)
    //       {
    //         getStatistics().fillHistogram("wrong_channel", channelNumber);
    //       }
    //       continue;
    //     }
    //
    //     auto& channel = getParamBank().getChannel(channelNumber);
    //     double synchroOffset =
    //         fConstansTree.get("pm_thr_offsets." + to_string(channel.getPM().getID()) + "." + to_string(channel.getThresholdNumber()), 0.0);
    //
    //     // double time = hit.time / 1000.;
    //     double time = hit.time;
    //
    //     // time = time - (fMaxTime - fMinTime);
    //     // time *= -1.;
    //
    //     if (time < fMinTime || time > fMaxTime)
    //     {
    //       continue;
    //     }
    //
    //     auto channelSignal = TimeWindowCreatorTools::generateChannelSignal(
    //         time, channel, hit.is_falling_edge == 0 ? JPetChannelSignal::Leading : JPetChannelSignal::Trailing, synchroOffset);
    //
    //     singleChannelSignals[channel.getID()].push_back(channelSignal);
    //   }
    // }
    //
    // for (auto& chSigs : singleChannelSignals)
    // {
    //   TimeWindowCreatorTools::flagChannelSignals(chSigs.second, getStatistics(), fSaveControlHistos);
    //   // Sort Signal Channels in time
    //   TimeWindowCreatorTools::sortByTime(chSigs.second);
    //   allChannelSignals.insert(allChannelSignals.end(), chSigs.second.begin(), chSigs.second.end());
    // }
    // // Save result
    // saveChannelSignals(allChannelSignals);
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
      getStatistics().fillHistogram("chsig_time", channelSig.getTime());
      getStatistics().fillHistogram("channel_occ", channelSig.getChannel().getID());

      // if (channelSig.getEdgeType() == JPetChannelSignal::Leading)
      // {
      getStatistics().fillHistogram("pm_occ", channelSig.getChannel().getPM().getID());
      getStatistics().fillHistogram(Form("pm_occ_thr%d", channelSig.getChannel().getThresholdNumber()), channelSig.getChannel().getPM().getID());
      // }

      if (channelSig.getRecoFlag() == JPetRecoSignal::Good)
      {
        getStatistics().fillHistogram("reco_flags_chsig", 1);
      }
      else if (channelSig.getRecoFlag() == JPetRecoSignal::Corrupted)
      {
        getStatistics().fillHistogram("reco_flags_chsig", 2);
      }
      else if (channelSig.getRecoFlag() == JPetRecoSignal::Unknown)
      {
        getStatistics().fillHistogram("reco_flags_chsig", 3);
      }
    }
  }
}

void TimeWindowCreator::initialiseHistograms()
{
  // Channels and PMs IDs from Param Bank
  auto minChannelID = getParamBank().getChannels().begin()->first;
  auto maxChannelID = getParamBank().getChannels().rbegin()->first;

  auto minPMID = getParamBank().getPMs().begin()->first;
  auto maxPMID = getParamBank().getPMs().rbegin()->first;

  getStatistics().createHistogramWithAxes(
      new TH1D("channel_occ", "Channel occupancy", maxChannelID - minChannelID + 1, minChannelID - 0.5, maxChannelID + 0.5),
      "Channels Signal in Time Slot", "Number of Time Slots");

  getStatistics().createHistogramWithAxes(new TH1D("chsig_time", "Signal Channels Time", 200, 1.1 * fMinTime, 1.1 * fMaxTime),
                                          "Channels Signal in Time Slot", "Number of Time Slots");

  getStatistics().createHistogramWithAxes(new TH1D("chsig_tslot", "Signal Channels Per Time Slot", 100, 0.5, 100.5), "Channels Signal in Time Slot",
                                          "Number of Time Slots");

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

  getStatistics().createHistogramWithAxes(new TH1D("reco_flags_chsig", "Number of good and corrupted Channel Sigals created", 4, 0.5, 4.5), " ",
                                          "Number of Channel Signals");
  vector<pair<unsigned, string>> binLabels = {make_pair(1, "GOOD"), make_pair(2, "CORRUPTED"), make_pair(3, "UNKNOWN"), make_pair(4, " ")};
  getStatistics().setHistogramBinLabel("reco_flags_chsig", getStatistics().AxisLabel::kXaxis, binLabels);

  // Flagging histograms
  getStatistics().createHistogramWithAxes(new TH1D("filter_LT_tdiff", "LT time diff", 400, -200000.0, 200000.0), "Time Diff [ps]",
                                          "Number of LT pairs");

  getStatistics().createHistogramWithAxes(new TH1D("filter_LL_tdiff", "Time diff of LL pairs", 200, 0.0, 5000.0), "Time Diff [ps]",
                                          "Number of LL pairs");

  getStatistics().createHistogramWithAxes(new TH1D("filter_TT_tdiff", "Time diff of TT pairs", 200, 0.0, 50000.0), "Time Diff [ps]",
                                          "Number of TT pairs");

  getStatistics().createHistogramWithAxes(new TH1D("filter_LL_PM", "Number of LL found on PMs", maxPMID - minPMID + 1, minPMID - 0.5, maxPMID + 0.5),
                                          "PM ID", "Number of LL pairs");

  getStatistics().createHistogramWithAxes(new TH1D("filter_TT_PM", "Number of TT found on PMs", maxPMID - minPMID + 1, minPMID - 0.5, maxPMID + 0.5),
                                          "PM ID", "Number of TT pairs");

  getStatistics().createHistogramWithAxes(new TH1D("filter_LL_THR", "Number of found LL on Thresolds", 4, 0.5, 4.5), "THR Number",
                                          "Number of LL pairs");

  getStatistics().createHistogramWithAxes(new TH1D("filter_TT_THR", "Number of found TT on Thresolds", 4, 0.5, 4.5), "THR Number",
                                          "Number of TT pairs");
}
