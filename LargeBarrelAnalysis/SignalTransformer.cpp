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
 *  @file SignalTransformer.cpp
 */

#include "JPetWriter/JPetWriter.h"
#include "SignalTransformer.h"

using namespace jpet_options_tools;

/**
 * Constructor
 */
SignalTransformer::SignalTransformer(const char* name): JPetUserTask(name) {}

/**
 * Destructor
 */
SignalTransformer::~SignalTransformer() {}

/**
 * Init Signal Transformer
 */
bool SignalTransformer::init()
{
  INFO("Signal transforming started: Raw to Reco and Phys");
  fOutputEvents = new JPetTimeWindow("JPetPhysSignal");
  // Getting bool for using bad signals
  if (isOptionSet(fParams.getOptions(), kUseCorruptedSignalsParamKey)) {
    fUseCorruptedSignals = getOptionAsBool(fParams.getOptions(), kUseCorruptedSignalsParamKey);
    if(fUseCorruptedSignals){
      WARNING("Signal Transformer is using Corrupted Signals, as set by the user");
    } else{
      WARNING("Signal Transformer is NOT using Corrupted Signals, as set by the user");
    }
  } else {
    WARNING("Signal Transformer is not using Corrupted Signals (default option)");
  }
  // Getting bool for saving histograms
  if (isOptionSet(fParams.getOptions(), kSaveControlHistosParamKey)) {
    fSaveControlHistos = getOptionAsBool(fParams.getOptions(), kSaveControlHistosParamKey);
  }

  // Control histograms
  if(fSaveControlHistos) { initialiseHistograms(); }
  return true;
}

/**
 * Execution Signal Transformer
 */
bool SignalTransformer::exec()
{
  if(auto & timeWindow = dynamic_cast<const JPetTimeWindow* const>(fEvent)) {
    uint n = timeWindow->getNumberOfEvents();
    for(uint i=0;i<n;++i){
      const JPetRawSignal & currSignal = dynamic_cast<const JPetRawSignal&>(timeWindow->operator[](i));
      if(currSignal.getRecoFlag()==JPetRawSignal::Corrupted) {
        if(fSaveControlHistos) {
          getStatistics().getHisto1D("good_vs_bad_signals")->Fill(2);
        }
        if(!fUseCorruptedSignals) continue;
      } else if(fSaveControlHistos) {
        getStatistics().getHisto1D("good_vs_bad_signals")->Fill(1);
      }
      // Make Reco Signal from Raw Signal
      auto recoSignal = createRecoSignal(currSignal);
      // Make Phys Signal from Reco Signal and save
      auto physSignal = createPhysSignal(recoSignal);
      fOutputEvents->add<JPetPhysSignal>(physSignal);
    }
  }else return false;
  return true;
}

/**
 * Terminate Signal Transformer
 */
bool SignalTransformer::terminate()
{
  INFO("Signal transforming finished");
  return true;
}

/**
 * Method rewrites Raw Signal to Reco Signal. All fields set to -1.
 */
JPetRecoSignal SignalTransformer::createRecoSignal(const JPetRawSignal& rawSignal)
{
  JPetRecoSignal recoSignal;
  recoSignal.setRawSignal(rawSignal);
  recoSignal.setAmplitude(-1.0);
  recoSignal.setOffset(-1.0);
  recoSignal.setCharge(-1.0);
  recoSignal.setDelay(-1.0);
  return recoSignal;
}

/**
 * Method rewrites Reco Signal to Phys Signal.
 * Time of Signal set to time of the Leading Signal Channel at the lowest threshold.
 * Other fields are set to -1, quality fields set to 0.
 */
JPetPhysSignal SignalTransformer::createPhysSignal(const JPetRecoSignal& recoSignal)
{
  JPetPhysSignal physSignal;
  physSignal.setRecoSignal(recoSignal);
  physSignal.setPhe(-1.0);
  physSignal.setQualityOfPhe(0.0);
  physSignal.setQualityOfTime(0.0);
  std::vector<JPetSigCh> leadingSigChVec = recoSignal.getRawSignal().getPoints(
    JPetSigCh::Leading, JPetRawSignal::ByThrValue
  );
  physSignal.setTime(leadingSigChVec.at(0).getValue());
  if(fSaveControlHistos) {
    getStatistics().getHisto1D("thr_used_for_time")->Fill(leadingSigChVec.at(0).getThresholdNumber());
  }
  return physSignal;
}

/**
 * Init histograms
 */
void SignalTransformer::initialiseHistograms(){
  getStatistics().createHistogram(
    new TH1F("good_vs_bad_signals", "Number of good and corrupted signals created", 2, 0.5, 2.5)
  );
  getStatistics().getHisto1D("good_vs_bad_signals")->GetXaxis()->SetBinLabel(1,"GOOD");
  getStatistics().getHisto1D("good_vs_bad_signals")->GetXaxis()->SetBinLabel(2,"CORRUPTED");
  getStatistics().getHisto1D("good_vs_bad_signals")->GetYaxis()->SetTitle("Number of Signals");

  getStatistics().createHistogram(
    new TH1F("thr_used_for_time", "Number of THR that was used for Signal Time setting", 4, 0.5, 4.5)
  );
  getStatistics().getHisto1D("thr_used_for_time")->GetXaxis()->SetTitle("THR Number");
  getStatistics().getHisto1D("thr_used_for_time")->GetYaxis()->SetTitle("Number of Signals");
}
