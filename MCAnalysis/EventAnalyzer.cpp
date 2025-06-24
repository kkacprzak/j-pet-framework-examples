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
#include <JPetCommonTools/JPetCommonTools.h>

using namespace jpet_options_tools;
using namespace std;

EventAnalyzer::EventAnalyzer(const char *name) : JPetUserTask(name) {}

EventAnalyzer::~EventAnalyzer() {}

bool EventAnalyzer::init()
{
  INFO("Event analysis started.");

  // Output events type
  fOutputEvents = new JPetTimeWindowMC("JPetEvent", "JPetRawMCHit", "JPetMCDecayTree");
  // fOutputEvents = new JPetTimeWindow("JPetEvent");

  // Getting bools for saving histograms
  if (isOptionSet(fParams.getOptions(), kSaveControlHistosParamKey))
  {
    fSaveControlHistos = getOptionAsBool(fParams.getOptions(), kSaveControlHistosParamKey);
  }

  // 3 gamma selection
  if (isOptionSet(fParams.getOptions(), k3gMinRelAngleParamKey))
  {
    f3gMinRelAngle1 = getOptionAsDouble(fParams.getOptions(), k3gMinRelAngleParamKey);
  }

  if (isOptionSet(fParams.getOptions(), kSave_SigBkgNTUParamKey))
  {
    fSaveNTU = getOptionAsBool(fParams.getOptions(), kSave_SigBkgNTUParamKey);
  }

  if(fSaveNTU)
  {
    auto inputFileName = getOptionAsString(fParams.getOptions(), "inputFile_std::string");

    // fSigOutFile = new TFile(fSigOutFileName.c_str(), "RECREATE");
    fSigOutTree = new TTree("signal", "JPET Signal Events");
    fSigOutTree->Branch("nhits", &fSigNumberOfHits, "nhits/I");
    fSigOutTree->Branch("times", &fSigHitTimes);
    fSigOutTree->Branch("pos", &fSigHitPos);
    fSigOutTree->Branch("tots", &fSigHitTOTs);
    fSigOutTree->Branch("scins", &fSigHitScinIDs);

    // fBkgOutFile = new TFile(fBkgOutFileName.c_str(), "RECREATE");
    fBkgOutTree = new TTree("background", "JPET Background Events");
    fBkgOutTree->Branch("nhits", &fBkgNumberOfHits, "nhits/I");
    fBkgOutTree->Branch("times", &fBkgHitTimes);
    fBkgOutTree->Branch("pos", &fBkgHitPos);
    fBkgOutTree->Branch("tots", &fBkgHitTOTs);
    fBkgOutTree->Branch("scins", &fBkgHitScinIDs);
  }

  if (fSaveControlHistos)
  {
    // Histograms for 3 gamma events
    getStatistics().createHistogramWithAxes(
        new TH2D("3g_rel_angles_signal", "Sum vs. difference of two smallest relative angles in 3 gamma signal event", 250, 0.0, 250, 200, 0.0, 200.0),
        "ang1+ang2 [deg]", "ang2-ang1 [deg]");

    getStatistics().createHistogramWithAxes(
        new TH2D("3g_rel_angles_background", "Sum vs. difference of two smallest relative angles in 3 gamma background event", 250, 0.0, 250, 200, 0.0, 200.0),
        "ang1+ang2 [deg]", "ang2-ang1 [deg]");

    getStatistics().createHistogramWithAxes(
        new TH2D("3g_rel_angles_signal_cut1", "Sum vs. difference of two smallest relative angles in 3 gamma signal event - after cut", 250, 0.0, 250, 200, 0.0, 200.0),
        "ang1+ang2 [deg]", "ang2-ang1 [deg]");

    getStatistics().createHistogramWithAxes(
        new TH2D("3g_rel_angles_background_cut1", "Sum vs. difference of two smallest relative angles in 3 gamma background event - after cut", 250, 0.0, 250, 200, 0.0, 200.0),
        "ang1+ang2 [deg]", "ang2-ang1 [deg]");

    getStatistics().createHistogramWithAxes(
        new TH2D("3g_rel_angles_signal_cut2", "Sum vs. difference of two smallest relative angles in 3 gamma signal event - after cut", 250, 0.0, 250, 200, 0.0, 200.0),
        "ang1+ang2 [deg]", "ang2-ang1 [deg]");

    getStatistics().createHistogramWithAxes(
        new TH2D("3g_rel_angles_background_cut2", "Sum vs. difference of two smallest relative angles in 3 gamma background event - after cut", 250, 0.0, 250, 200, 0.0, 200.0),
        "ang1+ang2 [deg]", "ang2-ang1 [deg]");

    getStatistics().createHistogramWithAxes(
        new TH2D("scatter_angle_time_signal", "Scatter angle vs. scatter test measure", 201, -6000.0, 6000.0, 181, -0.5, 180.5), "Time Diff [ps]", "Scatter angle");

    getStatistics().createHistogramWithAxes(
        new TH2D("scatter_angle_time_background", "Scatter angle vs. scatter test measure", 201, -6000.0, 6000.0, 181, -0.5, 180.5), "Time Diff [ps]", "Scatter angle");
  }

  return true;
}

bool EventAnalyzer::exec()
{
  if (auto timeWindow = dynamic_cast<const JPetTimeWindow *const>(fEvent))
  {
    for (uint i = 0; i < timeWindow->getNumberOfEvents(); i++)
    {
      const auto &event = dynamic_cast<const JPetEvent &>(timeWindow->operator[](i));

      int hits_number = event.getHits().size();
      if (hits_number < 3)
      {
        continue;
      }
      if (event.getRecoFlag() != JPetEvent::MC)
      {
        continue;
      }

      JPetTimeWindowMC *timeWindowMC = dynamic_cast<JPetTimeWindowMC *const>(fEvent);

      // Iterating over all the combinations of 3 hits
      for (int i = 0; i < hits_number; i++)
      {
        for (int j = i + 1; j < hits_number; j++)
        {
          for (int k = j + 1; k < hits_number; k++)
          {
            auto recoHit1 = dynamic_cast<const JPetMCRecoHit *>(event.getHits().at(i));
            auto recoHit2 = dynamic_cast<const JPetMCRecoHit *>(event.getHits().at(j));
            auto recoHit3 = dynamic_cast<const JPetMCRecoHit *>(event.getHits().at(k));

            auto mc_hit1 = timeWindowMC->getMCHit<JPetRawMCHit>(recoHit1->getMCindex());
            auto mc_hit2 = timeWindowMC->getMCHit<JPetRawMCHit>(recoHit2->getMCindex());
            auto mc_hit3 = timeWindowMC->getMCHit<JPetRawMCHit>(recoHit3->getMCindex());

            auto relAngesVec = getRelAngles(recoHit1->getPos(), recoHit2->getPos(), recoHit3->getPos());

            bool isSignal = false;
            // Signal events when all 3 are coming from oPs decay
            if (mc_hit1.getGammaTag() == 3 && mc_hit2.getGammaTag() == 3 && mc_hit3.getGammaTag() == 3)
            {
              isSignal = true;
              if(fSaveNTU)
              { 
                fSigNumberOfHits = 3;
                
                // Writing time in nanoseconds
                fSigHitTimes.push_back(recoHit1->getTime() / 1000.);
                fSigHitTimes.push_back(recoHit2->getTime() / 1000.);
                fSigHitTimes.push_back(recoHit3->getTime() / 1000.);
                
                fSigHitPos.push_back(recoHit1->getPos());
                fSigHitPos.push_back(recoHit2->getPos());
                fSigHitPos.push_back(recoHit3->getPos());

                fSigHitTOTs.push_back(recoHit1->getEnergy());
                fSigHitTOTs.push_back(recoHit2->getEnergy());
                fSigHitTOTs.push_back(recoHit3->getEnergy());

                fSigHitScinIDs.push_back(recoHit1->getScin().getID());
                fSigHitScinIDs.push_back(recoHit2->getScin().getID());
                fSigHitScinIDs.push_back(recoHit3->getScin().getID());

                fSigOutTree->Fill();
                resetRowSig();
              }


              if (fSaveControlHistos)
              {
                getStatistics().fillHistogram("3g_rel_angles_signal", relAngesVec.at(1) + relAngesVec.at(0), relAngesVec.at(1) - relAngesVec.at(0));
                getStatistics().fillHistogram("scatter_angle_time_signal",
                                              recoHit2->getTime() - recoHit1->getTime() - EventCategorizerTools::calculateScatteringTime(recoHit1, recoHit2),
                                              EventCategorizerTools::calculateScatteringAngle(recoHit1, recoHit2));

                getStatistics().fillHistogram("scatter_angle_time_signal",
                                              recoHit3->getTime() - recoHit1->getTime() - EventCategorizerTools::calculateScatteringTime(recoHit1, recoHit3),
                                              EventCategorizerTools::calculateScatteringAngle(recoHit1, recoHit3));

                getStatistics().fillHistogram("scatter_angle_time_signal",
                                              recoHit3->getTime() - recoHit2->getTime() - EventCategorizerTools::calculateScatteringTime(recoHit2, recoHit3),
                                              EventCategorizerTools::calculateScatteringAngle(recoHit2, recoHit3));

                if (relAngesVec.at(1) + relAngesVec.at(0) > f3gMinRelAngle1)
                {
                  getStatistics().fillHistogram("3g_rel_angles_signal_cut1", relAngesVec.at(1) + relAngesVec.at(0), relAngesVec.at(1) - relAngesVec.at(0));
                }
                if (relAngesVec.at(1) + relAngesVec.at(0) > f3gMinRelAngle2)
                {
                  getStatistics().fillHistogram("3g_rel_angles_signal_cut2", relAngesVec.at(1) + relAngesVec.at(0), relAngesVec.at(1) - relAngesVec.at(0));
                }
              }
            }
            else
            {
              if(fSaveNTU)
              { 
                fBkgNumberOfHits = 3;
                
                // Writing time in nanoseconds
                fBkgHitTimes.push_back(recoHit1->getTime() / 1000.);
                fBkgHitTimes.push_back(recoHit2->getTime() / 1000.);
                fBkgHitTimes.push_back(recoHit3->getTime() / 1000.);
                
                fBkgHitPos.push_back(recoHit1->getPos());
                fBkgHitPos.push_back(recoHit2->getPos());
                fBkgHitPos.push_back(recoHit3->getPos());

                fBkgHitTOTs.push_back(recoHit1->getEnergy());
                fBkgHitTOTs.push_back(recoHit2->getEnergy());
                fBkgHitTOTs.push_back(recoHit3->getEnergy());

                fBkgHitScinIDs.push_back(recoHit1->getScin().getID());
                fBkgHitScinIDs.push_back(recoHit2->getScin().getID());
                fBkgHitScinIDs.push_back(recoHit3->getScin().getID());

                fBkgOutTree->Fill();
                resetRowBkg();
              }
              if (fSaveControlHistos)
              {
                getStatistics().fillHistogram("3g_rel_angles_background", relAngesVec.at(1) + relAngesVec.at(0), relAngesVec.at(1) - relAngesVec.at(0));
                getStatistics().fillHistogram("scatter_angle_time_background",
                                              recoHit2->getTime() - recoHit1->getTime() - EventCategorizerTools::calculateScatteringTime(recoHit1, recoHit2),
                                              EventCategorizerTools::calculateScatteringAngle(recoHit1, recoHit2));

                getStatistics().fillHistogram("scatter_angle_time_background",
                                              recoHit3->getTime() - recoHit1->getTime() - EventCategorizerTools::calculateScatteringTime(recoHit1, recoHit3),
                                              EventCategorizerTools::calculateScatteringAngle(recoHit1, recoHit3));

                getStatistics().fillHistogram("scatter_angle_time_background",
                                              recoHit3->getTime() - recoHit2->getTime() - EventCategorizerTools::calculateScatteringTime(recoHit2, recoHit3),
                                              EventCategorizerTools::calculateScatteringAngle(recoHit2, recoHit3));

                if (relAngesVec.at(1) + relAngesVec.at(0) > f3gMinRelAngle1)
                {
                  getStatistics().fillHistogram("3g_rel_angles_background_cut1", relAngesVec.at(1) + relAngesVec.at(0), relAngesVec.at(1) - relAngesVec.at(0));
                }
                if (relAngesVec.at(1) + relAngesVec.at(0) > f3gMinRelAngle2)
                {
                  getStatistics().fillHistogram("3g_rel_angles_background_cut2", relAngesVec.at(1) + relAngesVec.at(0), relAngesVec.at(1) - relAngesVec.at(0));
                }
              }
            }
          }
        }
      }
    
      // Saving the event without modifications
      dynamic_cast<JPetTimeWindow*>(fOutputEvents)->add<JPetEvent>(event);
      // Rewriting MC
      for (int i = 0; i < timeWindowMC->getNumberOfMCHits(); ++i)
      {
        auto mcHit = timeWindowMC->getMCHit<JPetRawMCHit>(i);
        dynamic_cast<JPetTimeWindowMC*>(fOutputEvents)->addMCHit<JPetRawMCHit>(mcHit);
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

  fSigOutTree->Write();
  fBkgOutTree->Write();

  return true;
}

vector<double> EventAnalyzer::getRelAngles(TVector3 pos1, TVector3 pos2, TVector3 pos3)
{
  vector<double> relativeAngles;
  relativeAngles.push_back(TMath::RadToDeg() * pos1.Angle(pos2));
  relativeAngles.push_back(TMath::RadToDeg() * pos2.Angle(pos3));
  relativeAngles.push_back(TMath::RadToDeg() * pos3.Angle(pos1));
  sort(relativeAngles.begin(), relativeAngles.end());

  return relativeAngles;
}

void EventAnalyzer::resetRowSig()
{
  fSigNumberOfHits = 0;
  fSigHitTimes.clear();
  fSigHitPos.clear();
  fSigHitTOTs.clear();
  fSigHitScinIDs.clear();
}

void EventAnalyzer::resetRowBkg()
{
  fBkgNumberOfHits = 0;
  fBkgHitTimes.clear();
  fBkgHitPos.clear();
  fBkgHitTOTs.clear();
  fBkgHitScinIDs.clear();
}