/**
 *  @copyright Copyright 2024 The J-PET Framework Authors. All rights reserved.
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may find a copy of the License in the LICENCE file.
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  @file SignalFinderTools.cpp
 */

#include "SignalFinderTools.h"
#include <TRandom.h>

using namespace std;

const SignalFinderTools::Permutation SignalFinderTools::kIdentity = {0, 1, 2, 3};

/**
 * Method returns a map of vectors of JPetChannelSignal ordered by photomultiplier ID
 */
const map<int, vector<JPetChannelSignal>> SignalFinderTools::getChannelSignalsByPM(const JPetTimeWindow* timeWindow, bool useCorruptedChannelSignal,
                                                                                   int refPMID)
{
  map<int, vector<JPetChannelSignal>> chSigsPMMap;
  if (!timeWindow)
  {
    WARNING("Pointer of Time Window object is not set, returning empty map");
    return chSigsPMMap;
  }
  // Map Signal Channels according to PM they belong to
  const unsigned int nChannelSignals = timeWindow->getNumberOfEvents();
  for (unsigned int i = 0; i < nChannelSignals; i++)
  {
    auto chSig = dynamic_cast<const JPetChannelSignal&>(timeWindow->operator[](i));
    auto pmID = chSig.getChannel().getPM().getID();
    if (pmID == refPMID)
    {
      chSig.setRecoFlag(JPetRecoSignal::Good);
    }

    if (!useCorruptedChannelSignal && chSig.getRecoFlag() == JPetRecoSignal::Corrupted)
    {
      continue;
    }
    auto search = chSigsPMMap.find(pmID);
    if (search == chSigsPMMap.end())
    {
      vector<JPetChannelSignal> tmp;
      tmp.push_back(chSig);
      chSigsPMMap.insert(pair<int, vector<JPetChannelSignal>>(pmID, tmp));
    }
    else
    {
      search->second.push_back(chSig);
    }
  }
  return chSigsPMMap;
}

/**
 * Method invoking PM Signal building method for each PM separately
 */
vector<JPetPMSignal> SignalFinderTools::buildAllSignals(const map<int, vector<JPetChannelSignal>>& chSigByPM, double chSigEdgeMaxTime,
                                                        double chSigLeadTrailMaxTime, int numberOfThrs, JPetStatistics& stats, bool saveHistos,
                                                        SignalFinderTools::ToTCalculationType type, boost::property_tree::ptree& calibTree,
                                                        ThresholdOrderings thresholdOrderings)
{
  vector<JPetPMSignal> allSignals;

  for (auto& chSigPair : chSigByPM)
  {
    Permutation P;
    if (thresholdOrderings.empty())
    {
      P = kIdentity;
    }
    else
    {
      P = thresholdOrderings.at(chSigPair.first);
    }

    auto signals = buildPMSignals(chSigPair.second, chSigEdgeMaxTime, chSigLeadTrailMaxTime, numberOfThrs, stats, saveHistos, type, calibTree, P);
    allSignals.insert(allSignals.end(), signals.begin(), signals.end());
  }
  return allSignals;
}

/**
 * @brief Reconstruction of PM Signals based on Signal Channels on the same PM
 *
 * PMSignal is created with all Leading ChannelSignals that are found within first
 * time window (chSigEdgeMaxTime parameter) and all Trailing ChannelSignals that conform
 * to second time window (chSigLeadTrailMaxTime parameter).
 */
vector<JPetPMSignal> SignalFinderTools::buildPMSignals(const vector<JPetChannelSignal>& chSigByPM, double chSigEdgeMaxTime,
                                                       double chSigLeadTrailMaxTime, int numberOfThrs, JPetStatistics& stats, bool saveHistos,
                                                       SignalFinderTools::ToTCalculationType type, boost::property_tree::ptree& calibTree,
                                                       Permutation ordering)
{
  vector<JPetPMSignal> pmSigVec;
  vector<JPetChannelSignal> unusedLeads;

  vector<JPetChannelSignal> tmpVec;
  vector<vector<JPetChannelSignal>> leadChSigs(numberOfThrs, tmpVec);
  vector<vector<JPetChannelSignal>> trailChSigs(numberOfThrs, tmpVec);

  for (const JPetChannelSignal& chSig : chSigByPM)
  {
    if (chSig.getEdgeType() == JPetChannelSignal::Leading)
    {
      leadChSigs.at(ordering[chSig.getChannel().getThresholdNumber() - 1]).push_back(chSig);
    }
    else if (chSig.getEdgeType() == JPetChannelSignal::Trailing)
    {
      trailChSigs.at(ordering[chSig.getChannel().getThresholdNumber() - 1]).push_back(chSig);
    }
  }

  assert(leadChSigs.size() > 0);
  while (leadChSigs.at(0).size() > 0)
  {
    int closestTrailingChannelSignalTHR1 = findTrailingChannelSignal(leadChSigs.at(0).at(0), chSigLeadTrailMaxTime, trailChSigs.at(0));

    if (closestTrailingChannelSignalTHR1 == -1)
    {
      // Remains unused
      unusedLeads.push_back(leadChSigs.at(0).at(0));
      leadChSigs.at(0).erase(leadChSigs.at(0).begin());
      continue;
    }

    // PM Signal is created if the Lead-Trail pair is found at THR 1
    JPetPMSignal pmSig;
    pmSig.setPM(leadChSigs.at(0).at(0).getChannel().getPM());
    pmSig.setRecoFlag(JPetRecoSignal::Good);

    if (!pmSig.addLeadTrailPair(leadChSigs.at(0).at(0), trailChSigs.at(0).at(closestTrailingChannelSignalTHR1)))
    {
      // Remains unused
      unusedLeads.push_back(leadChSigs.at(0).at(0));
      leadChSigs.at(0).erase(leadChSigs.at(0).begin());
      continue;
    }

    if (saveHistos)
    {
      double tDiffTOT = trailChSigs.at(0).at(closestTrailingChannelSignalTHR1).getTime() - leadChSigs.at(0).at(0).getTime();
      stats.fillHistogram("lead_trail_thr1_diff", tDiffTOT);
      stats.fillHistogram("lead_trail_thr1_diff_pm", pmSig.getPM().getID(), tDiffTOT);
    }

    // Modifying flag if needed
    if (leadChSigs.at(0).at(0).getRecoFlag() == JPetRecoSignal::Corrupted ||
        trailChSigs.at(0).at(closestTrailingChannelSignalTHR1).getRecoFlag() == JPetRecoSignal::Corrupted)
    {
      pmSig.setRecoFlag(JPetRecoSignal::Corrupted);
    }

    // Adding Lead-Trail pairs if found on other THR
    for (unsigned int kk = 1; kk < numberOfThrs; kk++)
    {
      int nextThrChannelSignalIndex = findChannelSignalOnNextThr(leadChSigs.at(0).at(0).getTime(), chSigEdgeMaxTime, leadChSigs.at(kk));

      if (nextThrChannelSignalIndex != -1)
      {
        int closestTrailingChannelSignal =
            findTrailingChannelSignal(leadChSigs.at(kk).at(nextThrChannelSignalIndex), chSigLeadTrailMaxTime, trailChSigs.at(kk));
        if (closestTrailingChannelSignal != -1)
        {
          if (pmSig.addLeadTrailPair(leadChSigs.at(kk).at(nextThrChannelSignalIndex), trailChSigs.at(kk).at(closestTrailingChannelSignal)))
          {
            if (saveHistos)
            {
              double tDiffTHR = leadChSigs.at(kk).at(nextThrChannelSignalIndex).getTime() - leadChSigs.at(0).at(0).getTime();
              double tDiffTOT =
                  trailChSigs.at(kk).at(closestTrailingChannelSignal).getTime() - leadChSigs.at(kk).at(nextThrChannelSignalIndex).getTime();

              stats.fillHistogram(Form("lead_thr1_thr%d_diff", kk + 1), tDiffTHR);
              stats.fillHistogram(Form("lead_trail_thr%d_diff", kk + 1), tDiffTOT);

              stats.fillHistogram(Form("lead_thr1_thr%d_diff_pm", kk + 1), pmSig.getPM().getID(), tDiffTHR);
              stats.fillHistogram(Form("lead_trail_thr%d_diff_pm", kk + 1), pmSig.getPM().getID(), tDiffTOT);
            }

            // Modifying flag if needed
            if (leadChSigs.at(kk).at(nextThrChannelSignalIndex).getRecoFlag() == JPetRecoSignal::Corrupted ||
                trailChSigs.at(kk).at(closestTrailingChannelSignal).getRecoFlag() == JPetRecoSignal::Corrupted)
            {
              pmSig.setRecoFlag(JPetRecoSignal::Corrupted);
            }

            trailChSigs.at(kk).erase(trailChSigs.at(kk).begin() + closestTrailingChannelSignal);
            leadChSigs.at(kk).erase(leadChSigs.at(kk).begin() + nextThrChannelSignalIndex);
          }
        }
      }
    }

    // Finish bulding this signal
    pmSig.setTime(leadChSigs.at(0).at(0).getTime());
    pmSig.setToT(calculatePMSignalToT(pmSig, type, calibTree));
    pmSigVec.push_back(pmSig);

    trailChSigs.at(0).erase(trailChSigs.at(0).begin() + closestTrailingChannelSignalTHR1);
    leadChSigs.at(0).erase(leadChSigs.at(0).begin());

    // Filling control histograms
    if (saveHistos && gRandom->Uniform() < 0.001)
    {
      if (pmSig.getRecoFlag() == JPetRecoSignal::Good)
      {
        stats.fillHistogram("reco_flags_pmsig", 1);
      }
      else if (pmSig.getRecoFlag() == JPetRecoSignal::Corrupted)
      {
        stats.fillHistogram("reco_flags_pmsig", 2);
      }
      else
      {
        stats.fillHistogram("reco_flags_pmsig", 3);
      }

      for (auto chSig : unusedLeads)
      {
        stats.fillHistogram("unused_chsig_thr", 2 * chSig.getChannel().getThresholdNumber() - 1);
        stats.fillHistogram("unused_chsig_pm", chSig.getChannel().getPM().getID());
      }

      for (int jj = 0; jj < numberOfThrs; jj++)
      {
        for (auto chSig : leadChSigs.at(jj))
        {
          stats.fillHistogram("unused_chsig_thr", 2 * chSig.getChannel().getThresholdNumber() - 1);
          stats.fillHistogram("unused_chsig_pm", chSig.getChannel().getPM().getID());
        }
        for (auto chSig : trailChSigs.at(jj))
        {
          stats.fillHistogram("unused_chsig_thr", 2 * chSig.getChannel().getThresholdNumber());
          stats.fillHistogram("unused_chsig_pm", chSig.getChannel().getPM().getID());
        }
      }
    }
  }

  return pmSigVec;
}

/**
 * Method finds Signal Channels that belong to the same leading edge
 */
int SignalFinderTools::findChannelSignalOnNextThr(double chSigTime, double chSigEdgeMaxTime, const vector<JPetChannelSignal>& chSigVec)
{
  for (size_t i = 0; i < chSigVec.size(); i++)
  {
    if (fabs(chSigTime - chSigVec.at(i).getTime()) < chSigEdgeMaxTime)
    {
      return i;
    }
  }
  return -1;
}

/**
 * Method finds trailing edge Signal Channel that suits certian leading edge
 * Signal Channel, if more than one trailing edge Signal Channel found,
 * returning the one with the smallest index, that is equivalent of ChannelSignal
 * earliest in time
 */
int SignalFinderTools::findTrailingChannelSignal(const JPetChannelSignal& leadingChannelSignal, double chSigLeadTrailMaxTime,
                                                 const vector<JPetChannelSignal>& trailingChannelSignalVec)
{
  vector<int> trailingFoundIdices;
  for (size_t i = 0; i < trailingChannelSignalVec.size(); i++)
  {
    double timeDiff = trailingChannelSignalVec.at(i).getTime() - leadingChannelSignal.getTime();
    if (timeDiff > 0.0 && timeDiff < chSigLeadTrailMaxTime)
    {
      trailingFoundIdices.push_back(i);
    }
  }
  if (trailingFoundIdices.size() == 0)
  {
    return -1;
  }
  sort(trailingFoundIdices.begin(), trailingFoundIdices.end());
  return trailingFoundIdices.at(0);
}

double SignalFinderTools::calculatePMSignalToT(JPetPMSignal& pmSignal, SignalFinderTools::ToTCalculationType type,
                                               boost::property_tree::ptree& calibTree)
{
  double tot = 0.0;
  auto signals = pmSignal.getLeadTrailPairs();

  for (unsigned int thr = 0; thr < signals.size(); ++thr)
  {
    double prevTHR = (thr > 0) ? signals.at(thr - 1).first.getChannel().getThresholdValue() : 0.0;
    double thisTHR = signals.at(thr).first.getChannel().getThresholdValue();
    double thrDiff = thisTHR - prevTHR;
    double thisTDiff = signals.at(thr).second.getTime() - signals.at(thr).first.getTime();
    double prevTDiff = (thr > 0) ? signals.at(thr - 1).second.getTime() - signals.at(thr - 1).first.getTime() : thisTDiff;

    switch (type)
    {
    case kSimplified:
      tot += signals.at(thr).second.getTime() - signals.at(thr).first.getTime();
      break;
    case kThresholdRectangular:
      tot += thrDiff * thisTDiff;
      break;
    case kThresholdTrapeze:
      tot += 0.5 * (thisTDiff + prevTDiff) * thrDiff;
    }
  }

  // Applying ToT normalization constatns
  double totNormA = calibTree.get("sipm." + to_string(pmSignal.getPM().getID()) + ".tot_factor_a", 1.0);
  double totNormB = calibTree.get("sipm." + to_string(pmSignal.getPM().getID()) + ".tot_factor_b", 0.0);
  return tot * totNormA + totNormB;
}

/**
 * Method finds a 4-element permutation which has to be applied to threshold numbers
 * to have them sorted by increasing threshold values.
 *
 * The ordering may be different for each PMT, therefore the method creates a map
 * with PMT ID numbers as keys and 4-element permutations as values.
 */
SignalFinderTools::ThresholdOrderings SignalFinderTools::findThresholdOrder(const JPetParamBank& bank)
{
  ThresholdOrderings orderings;
  std::map<PMID, ThresholdValues> thr_values_per_pm;

  for (auto& channel : bank.getChannels())
  {
    PMID pmID = channel.second->getPM().getID();

    if (channel.second->getThresholdNumber() > kMaxNumberOfThresholds)
    {
      ERROR("Threshold number in configuration is larger than maximum.");
      return orderings;
    }

    thr_values_per_pm[pmID][channel.second->getThresholdNumber() - 1] = channel.second->getThresholdValue();
  }

  for (auto& pm : thr_values_per_pm)
  {
    permuteThresholdsByValue(pm.second, orderings[pm.first]);
  }

  return orderings;
}

/**
 * Helper method for findThresholdOrders which constructs a single permutation
 * based on the threshold values on a single PMT
 *
 * @param threshold_values array of floating-point values of voltage thresholds set on
 * front-end thresholds 1-4
 * @param new_ordering a permutation of thresholds 1-4 such that new_ordering[k] indicates the place of
 * threshold no. k (k in 0,1,2,3) in an array of thresholds sorted by voltage value
 */
void SignalFinderTools::permuteThresholdsByValue(const ThresholdValues& thresholdValues, Permutation& newOrdering)
{
  Permutation indices = kIdentity;

  sort(indices.begin(), indices.end(), [&](const int& a, const int& b) { return (thresholdValues.at(a) < thresholdValues.at(b)); });

  for (unsigned short i = 0; i < kMaxNumberOfThresholds; ++i)
  {
    newOrdering[indices[i]] = i;
  }
}
