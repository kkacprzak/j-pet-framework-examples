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
 *  @file EventEvaluator.cpp
 */

#include "EventEvaluator.h"
#include "../ModularDetectorAnalysis/EventCategorizerTools.h"
#include <JPetOptionsTools/JPetOptionsTools.h>
#include <JPetCommonTools/JPetCommonTools.h>
#include <TMVA/Tools.h>

using namespace jpet_options_tools;
using namespace std;

EventEvaluator::EventEvaluator(const char *name) : JPetUserTask(name) {}

EventEvaluator::~EventEvaluator() {}

bool EventEvaluator::init()
{
  INFO("Event analysis started.");

  // Output events type
  fOutputEvents = new JPetTimeWindowMC("JPetEvent", "JPetRawMCHit", "JPetMCDecayTree");

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

  // Path to TMVA dataset with weights
  if (isOptionSet(fParams.getOptions(), kPathToDatasetParamKey))
  {
    fPathToDataset = getOptionAsString(fParams.getOptions(), kPathToDatasetParamKey);
  }

  // Initialising TMVA
  TMVA::Tools::Instance();
  fReader = new TMVA::Reader("!Color:!Silent");

  fReader->AddVariable("relAng1", &fRelAng1);
  fReader->AddVariable("relAng2", &fRelAng2);
  fReader->AddVariable("relAng3", &fRelAng3);
  fReader->AddVariable("time21", &fTime21);
  fReader->AddVariable("time32", &fTime32);
  fReader->AddVariable("time31", &fTime31);
  fReader->AddVariable("tot1", &fTOT1);
  fReader->AddVariable("tot2", &fTOT2);
  fReader->AddVariable("tot3", &fTOT3);

  // Methods names and therir max significance cutoff
  fMethods["MLP"] = 0.9;
  fMethods["MLPBFGS"] = 0.9;
  fMethods["MLPBNN"] = 0.9;

  fMethods["BDT"] = 0.9;
  fMethods["BDTG"] = 0.9;
  fMethods["BDTB"] = 0.9;
  fMethods["BDTD"] = 0.9;
  fMethods["BDTF"] = 0.9;

  // Booking methods with TMVA reader
  for(auto method : fMethods)
  {
    std::string methodName = method.first + " method";
    std::string weightFile = fPathToDataset+"dataset/weights/TMVAClassification_"+std::string(method.first)+".weights.xml";
    fReader->BookMVA(methodName, weightFile);

    if (fSaveControlHistos)
    {
      getStatistics().createHistogramWithAxes(
        new TH1D(Form("%s_sig_acc", method.first), Form("%s_sig_acc", method.first), 200, -1.25, 1.25), 
        "evaluation parameter", "number of entries"
      );

      getStatistics().createHistogramWithAxes(
        new TH1D(Form("%s_sig_rej", method.first), Form("%s_sig_rej", method.first), 200, -1.25, 1.25), 
        "evaluation parameter", "number of entries"
      );

      getStatistics().createHistogramWithAxes(
        new TH1D(Form("%s_bkg_acc", method.first), Form("%s_bkg_acc", method.first), 200, -1.25, 1.25), 
        "evaluation parameter", "number of entries"
      );

      getStatistics().createHistogramWithAxes(
        new TH1D(Form("%s_bkg_rej", method.first), Form("%s_bkg_rej", method.first), 200, -1.25, 1.25), 
        "evaluation parameter", "number of entries"
      );

      getStatistics().createHistogramWithAxes(
        new TH2D(Form("%s_3g_sig_acc", method.first), Form("%s_3g_sig_acc", method.first), 250, 0.0, 250, 200, 0.0, 200.0),
        "ang1+ang2 [deg]", "ang2-ang1 [deg]");

      getStatistics().createHistogramWithAxes(
        new TH2D(Form("%s_3g_sig_rej", method.first), Form("%s_3g_sig_rej", method.first), 250, 0.0, 250, 200, 0.0, 200.0),
        "ang1+ang2 [deg]", "ang2-ang1 [deg]");

      getStatistics().createHistogramWithAxes(
        new TH2D(Form("%s_3g_bkg_acc", method.first), Form("%s_3g_bkg_acc", method.first), 250, 0.0, 250, 200, 0.0, 200.0),
        "ang1+ang2 [deg]", "ang2-ang1 [deg]");

      getStatistics().createHistogramWithAxes(
        new TH2D(Form("%s_3g_bkg_rej", method.first), Form("%s_3g_bkg_rej", method.first), 250, 0.0, 250, 200, 0.0, 200.0),
        "ang1+ang2 [deg]", "ang2-ang1 [deg]");    
    }    
  }

  return true;
}

bool EventEvaluator::exec()
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
            // Accessing hits
            auto recoHit1 = dynamic_cast<const JPetMCRecoHit *>(event.getHits().at(i));
            auto recoHit2 = dynamic_cast<const JPetMCRecoHit *>(event.getHits().at(j));
            auto recoHit3 = dynamic_cast<const JPetMCRecoHit *>(event.getHits().at(k));

            auto mc_hit1 = timeWindowMC->getMCHit<JPetRawMCHit>(recoHit1->getMCindex());
            auto mc_hit2 = timeWindowMC->getMCHit<JPetRawMCHit>(recoHit2->getMCindex());
            auto mc_hit3 = timeWindowMC->getMCHit<JPetRawMCHit>(recoHit3->getMCindex());

            bool isSignal = false;
            // Signal events when all 3 are coming from oPs decay
            if (mc_hit1.getGammaTag() == 3 && mc_hit2.getGammaTag() == 3 && mc_hit3.getGammaTag() == 3)
            {
              isSignal = true;
            }

            // input variables for TMVA Reader
            auto relAngesVec = getRelAngles(recoHit1->getPos(), recoHit2->getPos(), recoHit3->getPos());

            fRelAng1 = relAngesVec.at(0);
            fRelAng2 = relAngesVec.at(1);
            fRelAng3 = relAngesVec.at(2);
            fTime21 = getScatterTestMeasure(recoHit1, recoHit2);
            fTime32 = getScatterTestMeasure(recoHit2, recoHit3);
            fTime31 = getScatterTestMeasure(recoHit1, recoHit3);
            fTOT1 = recoHit1->getEnergy();
            fTOT2 = recoHit2->getEnergy();
            fTOT3 = recoHit3->getEnergy();

            // 3 hit evaluation with TMVA methods
            for(auto method : fMethods)
            {
              bool isAccepted = false;
              auto evaluation = fReader->EvaluateMVA(method.first+" method");
              if(evaluation > method.second)
              {
                isAccepted = true;
              }

              if(isSignal && isAccepted)
              {
                getStatistics().fillHistogram(Form("%s_sig_acc", method.first), evaluation);
                getStatistics().fillHistogram(Form("%s_3g_sig_acc", method.first), relAngesVec.at(1) + relAngesVec.at(0), relAngesVec.at(1) - relAngesVec.at(0));

              }
              if(isSignal && !isAccepted)
              {
                getStatistics().fillHistogram(Form("%s_sig_rej", method.first), evaluation);
                getStatistics().fillHistogram(Form("%s_3g_sig_rej", method.first), relAngesVec.at(1) + relAngesVec.at(0), relAngesVec.at(1) - relAngesVec.at(0));
              }
              if(!isSignal && isAccepted)
              {
                getStatistics().fillHistogram(Form("%s_bkg_acc", method.first), evaluation);
                getStatistics().fillHistogram(Form("%s_3g_bkg_acc", method.first), relAngesVec.at(1) + relAngesVec.at(0), relAngesVec.at(1) - relAngesVec.at(0));
              }
              if(!isSignal && !isAccepted)
              {
                getStatistics().fillHistogram(Form("%s_bkg_rej", method.first), evaluation);
                getStatistics().fillHistogram(Form("%s_3g_bkg_rej", method.first), relAngesVec.at(1) + relAngesVec.at(0), relAngesVec.at(1) - relAngesVec.at(0));
              }
            }            
          }
        }
      }
    
      // // Saving the event without modifications
      // dynamic_cast<JPetTimeWindow*>(fOutputEvents)->add<JPetEvent>(event);
      // // Rewriting MC
      // for (int i = 0; i < timeWindowMC->getNumberOfMCHits(); ++i)
      // {
      //   auto mcHit = timeWindowMC->getMCHit<JPetRawMCHit>(i);
      //   dynamic_cast<JPetTimeWindowMC*>(fOutputEvents)->addMCHit<JPetRawMCHit>(mcHit);
      // }
    }
  }
  else
  {
    return false;
  }

  return true;
}

bool EventEvaluator::terminate()
{
  INFO("Event evaluation with ML completed.");

  return true;
}

vector<float> EventEvaluator::getRelAngles(TVector3 pos1, TVector3 pos2, TVector3 pos3)
{
  vector<float> relativeAngles;
  relativeAngles.push_back(TMath::RadToDeg() * pos1.Angle(pos2));
  relativeAngles.push_back(TMath::RadToDeg() * pos2.Angle(pos3));
  relativeAngles.push_back(TMath::RadToDeg() * pos3.Angle(pos1));
  sort(relativeAngles.begin(), relativeAngles.end());

  return relativeAngles;
}

float EventEvaluator::getScatterTestMeasure(const JPetBaseHit* hit1, const JPetBaseHit* hit2)
{
  float distance = (hit1->getPos() - hit2->getPos()).Mag();
  float dt = distance / 29.9792458;
  return hit2->getTime() - hit1->getTime() - dt;
}
