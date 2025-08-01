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

    getStatistics().createHistogramWithAxes(new TH2D("3g_ap_xy_mc", "XY position of annihilation point (bin 0.5 cm)", 202, -50.5, 50.5, 202, -50.5, 50.5),
                                          "X position [cm]", "Y position [cm]");

    getStatistics().createHistogramWithAxes(new TH2D("3g_ap_zx_mc", "ZX position of annihilation point (bin 0.5 cm)", 202, -50.5, 50.5, 202, -50.5, 50.5),
                                            "Z position [cm]", "X position [cm]");

    getStatistics().createHistogramWithAxes(new TH2D("3g_ap_zy_mc", "ZY position of annihilation point (bin 0.5 cm)", 202, -50.5, 50.5, 202, -50.5, 50.5),
                                            "Z position [cm]", "Y position [cm]");

    getStatistics().createHistogramWithAxes(
        new TH2D("3g_ap_xy_mc_zoom", "XY position of annihilation point (bin 0.25 cm)", 132, -16.5, 16.5, 132, -16.5, 16.5), "X position [cm]",
        "Y position [cm]");

    getStatistics().createHistogramWithAxes(
        new TH2D("3g_ap_zx_mc_zoom", "ZX position of annihilation point (bin 0.25 cm)", 132, -16.5, 16.5, 132, -16.5, 16.5), "Z position [cm]",
        "X position [cm]");

    getStatistics().createHistogramWithAxes(
        new TH2D("3g_ap_zy_mc_zoom", "ZY position of annihilation point (bin 0.25 cm)", 132, -16.5, 16.5, 132, -16.5, 16.5), "Z position [cm]",
        "Y position [cm]");

    getStatistics().createHistogramWithAxes(new TH2D("3g_ap_xy_reco", "XY position of annihilation point (bin 0.5 cm)", 202, -50.5, 50.5, 202, -50.5, 50.5),
                                            "X position [cm]", "Y position [cm]");

    getStatistics().createHistogramWithAxes(new TH2D("3g_ap_zx_reco", "ZX position of annihilation point (bin 0.5 cm)", 202, -50.5, 50.5, 202, -50.5, 50.5),
                                            "Z position [cm]", "X position [cm]");

    getStatistics().createHistogramWithAxes(new TH2D("3g_ap_zy_reco", "ZY position of annihilation point (bin 0.5 cm)", 202, -50.5, 50.5, 202, -50.5, 50.5),
                                            "Z position [cm]", "Y position [cm]");

    getStatistics().createHistogramWithAxes(
        new TH2D("3g_ap_xy_reco_zoom", "XY position of annihilation point (bin 0.25 cm)", 132, -16.5, 16.5, 132, -16.5, 16.5), "X position [cm]",
        "Y position [cm]");

    getStatistics().createHistogramWithAxes(
        new TH2D("3g_ap_zx_reco_zoom", "ZX position of annihilation point (bin 0.25 cm)", 132, -16.5, 16.5, 132, -16.5, 16.5), "Z position [cm]",
        "X position [cm]");

    getStatistics().createHistogramWithAxes(
        new TH2D("3g_ap_zy_reco_zoom", "ZY position of annihilation point (bin 0.25 cm)", 132, -16.5, 16.5, 132, -16.5, 16.5), "Z position [cm]",
        "Y position [cm]");

    // Histograms for scattering category
    getStatistics().createHistogramWithAxes(new TH1D("scatter_test_dist_abs", "Scatter Test - Distance Difference", 201, 0.0, 120.0), "Dist Diff [cm]",
                                            "Number of Hit Pairs");

    getStatistics().createHistogramWithAxes(new TH1D("scatter_test_dist_rel", "Scatter Test - Distance Difference", 201, -120.0, 120.0),
                                            "Dist Diff [cm]", "Number of Hit Pairs");

    getStatistics().createHistogramWithAxes(new TH1D("scatter_test_time_abs", "Scatter Test - Time Difference", 201, 0.0, 10000.0), "Time Diff [ps]",
                                            "Number of Hit Pairs");

    getStatistics().createHistogramWithAxes(new TH1D("scatter_test_time_rel", "Scatter Test - Time Difference", 201, -5000.0, 5000.0), "Time Diff [ps]",
                                            "Number of Hit Pairs");

    getStatistics().createHistogramWithAxes(new TH1D("scatter_test_abs_pass", "Passed Scatter Test - Time Difference", 201, 0.0, 10000.0),
                                            "Time Diff [ps]", "Number of Hit Pairs");

    getStatistics().createHistogramWithAxes(new TH1D("scatter_test_abs_fail", "Failed Scatter Test - Time Difference", 201, 0.0, 10000.0),
                                            "Time Diff [ps]", "Number of Hit Pairs");

    getStatistics().createHistogramWithAxes(new TH1D("scatter_test_rel_pass", "Passed Scatter Test - Time Difference", 201, -5000.0, 5000.0),
                                            "Time Diff [ps]", "Number of Hit Pairs");

    getStatistics().createHistogramWithAxes(new TH1D("scatter_test_rel_fail", "Failed Scatter Test - Time Difference", 201, -5000.0, 5000.0),
                                            "Time Diff [ps]", "Number of Hit Pairs");

    getStatistics().createHistogramWithAxes(
        new TH2D("scatter_angle_time", "Scatter angle vs. scatter test measure", 201, -6000.0, 6000.0, 181, -0.5, 180.5), "Time Diff [ps]",
        "Scatter angle");

    getStatistics().createHistogramWithAxes(
        new TH2D("scatter_angle_time_small", "Scatter angle vs. scatter test measure", 201, -6000.0, 6000.0, 41, 139.5, 180.5), "Time Diff [ps]",
        "Scatter angle");

    getStatistics().createHistogramWithAxes(
        new TH2D("scatter_angle_time_pass", "Passed Scatter angle vs. scatter test measure", 201, -6000.0, 6000.0, 181, -0.5, 180.5), "Time Diff [ps]",
        "Scatter angle");

    getStatistics().createHistogramWithAxes(
        new TH2D("scatter_angle_time_fail", "Failed Scatter angle vs. scatter test measure", 201, -6000.0, 6000.0, 181, -0.5, 180.5), "Time Diff [ps]",
        "Scatter angle");
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
        // Filling resolution histograms
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

          //Scatter test
          if(k < event.getHits().size()-1)
          {
            auto reconstructedHit2 = dynamic_cast<const JPetMCRecoHit*>(event.getHits().at(k+1));
            if (!reconstructedHit2)
            {
              continue;
            }
            auto isScatter = EventCategorizerTools::checkForScatter(reconstructedHit, reconstructedHit2, getStatistics(), true, EventCategorizerTools::kMinMaxParams, 
                                                                    fScatterTimeMax, fScatterTimeMin, fScatterTimeMax, fScatterAngleMin, fScatterAngleMax);
          }
        }
        if (fSaveControlHistos)
        {
          getStatistics().fillHistogram("evt_ids_multi", evtIDs.size());
        }


        // oPs signal analysis
        if(event.getHits().size()==3)
        {
          auto recoHit1 = dynamic_cast<const JPetMCRecoHit *>(event.getHits().at(0));
          auto recoHit2 = dynamic_cast<const JPetMCRecoHit *>(event.getHits().at(1));
          auto recoHit3 = dynamic_cast<const JPetMCRecoHit *>(event.getHits().at(2));

          auto mcHit1 = timeWindowMC->getMCHit<JPetRawMCHit>(recoHit1->getMCindex());
          auto mcHit2 = timeWindowMC->getMCHit<JPetRawMCHit>(recoHit2->getMCindex());
          auto mcHit3 = timeWindowMC->getMCHit<JPetRawMCHit>(recoHit3->getMCindex());

          if (mcHit1.getGammaTag() == 3 && mcHit2.getGammaTag() == 3 && mcHit3.getGammaTag() == 3)
          {
            auto apPosMC = EventCategorizerTools::calculateAnnihilationPointByMinimization(mcHit1, mcHit2, mcHit3);
            // auto apTimePosMC = EventCategorizerTools::calculateAnnihilationPointAndTimeByTrilateration(mcHit1, mcHit2, mcHit3);
            auto apPosReco = EventCategorizerTools::calculateAnnihilationPointByMinimization(*recoHit1, *recoHit2, *recoHit3);
            // auto apTimePosReco = EventCategorizerTools::calculateAnnihilationPointAndTimeByTrilateration(*recoHit1, *recoHit2, *recoHit3);

            if (fSaveControlHistos)
            {
              // getStatistics().fillHistogram("3g_ap_xy_mc", apTimePosMC.second.X(), apTimePosMC.second.Y());
              // getStatistics().fillHistogram("3g_ap_zx_mc", apTimePosMC.second.Z(), apTimePosMC.second.X());
              // getStatistics().fillHistogram("3g_ap_zy_mc", apTimePosMC.second.Z(), apTimePosMC.second.Y());

              // getStatistics().fillHistogram("3g_ap_xy_mc_zoom", apTimePosMC.second.X(), apTimePosMC.second.Y());
              // getStatistics().fillHistogram("3g_ap_zx_mc_zoom", apTimePosMC.second.Z(), apTimePosMC.second.X());
              // getStatistics().fillHistogram("3g_ap_zy_mc_zoom", apTimePosMC.second.Z(), apTimePosMC.second.Y());

              // getStatistics().fillHistogram("3g_ap_xy_reco", apTimePosReco.second.X(), apTimePosReco.second.Y());
              // getStatistics().fillHistogram("3g_ap_zx_reco", apTimePosReco.second.Z(), apTimePosReco.second.X());
              // getStatistics().fillHistogram("3g_ap_zy_reco", apTimePosReco.second.Z(), apTimePosReco.second.Y());

              // getStatistics().fillHistogram("3g_ap_xy_reco_zoom", apTimePosReco.second.X(), apTimePosReco.second.Y());
              // getStatistics().fillHistogram("3g_ap_zx_reco_zoom", apTimePosReco.second.Z(), apTimePosReco.second.X());
              // getStatistics().fillHistogram("3g_ap_zy_reco_zoom", apTimePosReco.second.Z(), apTimePosReco.second.Y());

              getStatistics().fillHistogram("3g_ap_xy_mc", apPosMC.X(), apPosMC.Y());
              getStatistics().fillHistogram("3g_ap_zx_mc", apPosMC.Z(), apPosMC.X());
              getStatistics().fillHistogram("3g_ap_zy_mc", apPosMC.Z(), apPosMC.Y());

              getStatistics().fillHistogram("3g_ap_xy_mc_zoom", apPosMC.X(), apPosMC.Y());
              getStatistics().fillHistogram("3g_ap_zx_mc_zoom", apPosMC.Z(), apPosMC.X());
              getStatistics().fillHistogram("3g_ap_zy_mc_zoom", apPosMC.Z(), apPosMC.Y());

              getStatistics().fillHistogram("3g_ap_xy_reco", apPosReco.X(), apPosReco.Y());
              getStatistics().fillHistogram("3g_ap_zx_reco", apPosReco.Z(), apPosReco.X());
              getStatistics().fillHistogram("3g_ap_zy_reco", apPosReco.Z(), apPosReco.Y());

              getStatistics().fillHistogram("3g_ap_xy_reco_zoom", apPosReco.X(), apPosReco.Y());
              getStatistics().fillHistogram("3g_ap_zx_reco_zoom", apPosReco.Z(), apPosReco.X());
              getStatistics().fillHistogram("3g_ap_zy_reco_zoom", apPosReco.Z(), apPosReco.Y());
            }
          }
          
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
