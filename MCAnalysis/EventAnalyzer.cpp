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
 *  @file EventAnalyzer.cpp
 */

#include "EventAnalyzer.h"
#include "../CommonTools/EventCategorizerTools.h"
#include <Hits/JPetMCRecoHit/JPetMCRecoHit.h>
#include <JPetOptionsTools/JPetOptionsTools.h>
#include <JPetRawMCHit/JPetRawMCHit.h>

using namespace jpet_options_tools;
using namespace std;

EventAnalyzer::EventAnalyzer(const char* name) : JPetUserTask(name) {}

EventAnalyzer::~EventAnalyzer() {}

bool EventAnalyzer::init()
{
  INFO("Event analysis started.");

  // Getting bool for saving histograms
  if (isOptionSet(fParams.getOptions(), kSaveControlHistosParamKey))
  {
    fSaveControlHistos = getOptionAsBool(fParams.getOptions(), kSaveControlHistosParamKey);
  }

  // Initialize histograms
  if (fSaveControlHistos)
  {
    getStatistics().createHistogramWithAxes(new TH1D("z_res", "Resolution along Z", 301, -15.05, 15.05), "Z_{REC}-Z_{MC} [cm]");
    getStatistics().createHistogramWithAxes(new TH1D("Edep_res", "Resolution of deposited energy", 201, -201., 201.), "E_{REC}-E_{MC} [keV]");
    getStatistics().createHistogramWithAxes(new TH1D("evt_ids_multi", "Number of different events in event time window", 10, -0.5, 9.5),
                                            "Multiplicity of evt IDs", "Number of events");
  }

  // Output events type - time window with MonteCarlo data
  fOutputEvents = new JPetTimeWindowMC("JPetEvent", "JPetRawMCHit", "JPetMCDecayTree");
  return true;
}

bool EventAnalyzer::exec()
{
  // Identify whether the input events are MC or DATA.
  // In case of MC, store the pointer to the TimeWindowMC object
  // which contains "true MC" information about the generated events.
  JPetTimeWindowMC* timeWindowMC = nullptr;
  if (timeWindowMC = dynamic_cast<JPetTimeWindowMC* const>(fEvent))
  {
    fIsMC = true;
    INFO("The input file is MC.");
  }
  else
  {
    INFO("The input file is DATA.");
  }

  if (auto timeWindow = dynamic_cast<const JPetTimeWindow* const>(fEvent))
  {
    for (uint i = 0; i < timeWindow->getNumberOfEvents(); i++)
    {
      const auto& event = dynamic_cast<const JPetEvent&>(timeWindow->operator[](i));

      if (fIsMC)
      {
        vector<int> evtIDs;
        for (int k = 0; k < event.getHits().size(); ++k)
        {
          auto reconstructedHit = dynamic_cast<const JPetMCRecoHit*>(event.getHits().at(k));
          if (!reconstructedHit)
          {
            continue;
          }
          // for each reconstructed hit, we access the corresponding "true MC" hit
          const JPetRawMCHit& mcHit = timeWindowMC->getMCHit<JPetRawMCHit>(reconstructedHit->getMCindex());
          if (find(evtIDs.begin(), evtIDs.end(), mcHit.getMCVtxIndex()) == evtIDs.end())
          {
            evtIDs.push_back(mcHit.getMCVtxIndex());
          }

          if (fSaveControlHistos)
          {
            getStatistics().fillHistogram("z_res", reconstructedHit->getPos().Z() - mcHit.getPos().Z());
            getStatistics().fillHistogram("Edep_res", reconstructedHit->getEnergy() - mcHit.getEnergy());
          }
        }
        if (fSaveControlHistos)
        {
          getStatistics().fillHistogram("evt_ids_multi", evtIDs.size());
        }
      }

      fOutputEvents->add<JPetEvent>(event);
    }
  }
  else
  {
    return false;
  }

  return true;
}

bool EventAnalyzer::terminate()
{
  INFO("Event analysis completed.");
  return true;
}
