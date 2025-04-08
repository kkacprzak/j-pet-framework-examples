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
#include "../ModularDetectorAnalysis/EventCategorizerTools.h"
#include <JPetOptionsTools/JPetOptionsTools.h>

using namespace jpet_options_tools;
using namespace std;

EventAnalyzer::EventAnalyzer(const char* name) : JPetUserTask(name) {}

EventAnalyzer::~EventAnalyzer() {}

bool EventAnalyzer::init()
{
  INFO("Event analysis started.");

  // Input events type
  // fOutputEvents = new JPetTimeWindowMC("JPetEvent", "JPetRawMCHit", "JPetMCDecayTree");
  fOutputEvents = new JPetTimeWindow("JPetEvent");

  // Getting bools for saving histograms
  if (isOptionSet(fParams.getOptions(), kSaveControlHistosParamKey))
  {
    fSaveControlHistos = getOptionAsBool(fParams.getOptions(), kSaveControlHistosParamKey);
  }

  // 3 gamma selection
  if (isOptionSet(fParams.getOptions(), k3gMinRelAngleParamKey))
  {
    f3gMinRelAngle = getOptionAsDouble(fParams.getOptions(), k3gMinRelAngleParamKey);
  }

  if (isOptionSet(fParams.getOptions(), kSave_oPsOnlyParamKey))
  {
    fSave_oPsOnly = getOptionAsBool(fParams.getOptions(), kSave_oPsOnlyParamKey);
  }

  if (fSaveControlHistos)
  {
    getStatistics().createHistogramWithAxes(new TH1D("z_res", "Resolution along Z", 301, -15.05, 15.05), "Z_{REC}-Z_{MC} [cm]");

    getStatistics().createHistogramWithAxes(new TH1D("Edep_res", "Resolution of deposited energy", 201, -201., 201.), "E_{REC}-E_{MC} [keV]");

    // Histograms for 3 gamma events
    getStatistics().createHistogramWithAxes(
        new TH2D("3g_rel_angles", "Sum vs. difference of two smallest relative angles in 3 gamma event", 250, 0.0, 250, 200, 0.0, 200.0),
        "ang1+ang2 [deg]", "ang2-ang1 [deg]");

    getStatistics().createHistogramWithAxes(new TH2D("3g_rel_angles_sel",
                                                     "Sum vs. difference of two smallest relative angles in 3 gamma event - after cut", 250, 0.0, 250,
                                                     200, 0.0, 200.0),
                                            "ang1+ang2 [deg]", "ang2-ang1 [deg]");
  }

  return true;
}

bool EventAnalyzer::exec()
{
  if (auto timeWindow = dynamic_cast<const JPetTimeWindow* const>(fEvent))
  {
    for (uint i = 0; i < timeWindow->getNumberOfEvents(); i++)
    {
      const auto& event = dynamic_cast<const JPetEvent&>(timeWindow->operator[](i));

      // Identify whether the input events are MC or DATA.
      // In case of MC, store the pointer to the TimeWindowMC object
      // which contains "true MC" information about the generated events.
      if (event.getRecoFlag() == JPetEvent::MC)
      {
        fIsMC = true;
        JPetTimeWindowMC* timeWindowMC = dynamic_cast<JPetTimeWindowMC* const>(fEvent);

        // if the input is MC, we fill resolution histograms
        // to check if MC smearing works fine
        bool isPure_oPs = true;
        int hits_number = event.getHits().size();
        for (int k = 0; k < hits_number; ++k)
        {
          auto reconstructed_hit = dynamic_cast<const JPetMCRecoHit*>(event.getHits().at(k));
          if (!reconstructed_hit)
          {
            continue;
          }
          // for each reconstructed hit, we access the corresponding "true MC" hit
          const JPetRawMCHit& mc_hit = timeWindowMC->getMCHit<JPetRawMCHit>(reconstructed_hit->getMCindex());

          if (fSaveControlHistos)
          {
            fillResolutionHistograms(reconstructed_hit, mc_hit);
          }

          if (fSave_oPsOnly && mc_hit.getGammaTag() != 3)
          {
            isPure_oPs = false;
          }
        }

        if (fSave_oPsOnly && hits_number == 3 && isPure_oPs)
        {
          bool pass3angleCut = EventCategorizerTools::checkFor3Gamma(event, f3gMinRelAngle, getStatistics(), fSaveControlHistos);
          fOutputEvents->add<JPetEvent>(event);
        }

        // Save all events
        if (!fSave_oPsOnly)
        {
          fOutputEvents->add<JPetEvent>(event);
        }
      }
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

void EventAnalyzer::fillResolutionHistograms(const JPetMCRecoHit* reconstructed_hit, const JPetRawMCHit& mc_hit)
{
  getStatistics().fillHistogram("z_res", reconstructed_hit->getPos().Z() - mc_hit.getPos().Z());
  getStatistics().fillHistogram("Edep_res", reconstructed_hit->getEnergy() - mc_hit.getEnergy());
}
