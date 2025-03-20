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
 *  @file SignalTransformer.cpp
 */

#include "SignalTransformer.h"
#include "../CommonTools/SignalTransformerTools.h"
#include "JPetWriter/JPetWriter.h"
#include <boost/property_tree/json_parser.hpp>

using namespace jpet_options_tools;

SignalTransformer::SignalTransformer(const char* name) : JPetUserTask(name) {}

SignalTransformer::~SignalTransformer() {}

bool SignalTransformer::init()
{
  INFO("Signal transforming started: Raw to Reco and Phys");
  fOutputEvents = new JPetTimeWindow("JPetMatrixSignal");

  // Getting bool for using bad signals
  if (isOptionSet(fParams.getOptions(), kUseCorruptedSignalsParamKey))
  {
    fUseCorruptedSignals = getOptionAsBool(fParams.getOptions(), kUseCorruptedSignalsParamKey);
    if (fUseCorruptedSignals)
    {
      INFO("Signal Transformer is using Corrupted Signals, as set by the user");
    }
    else
    {
      INFO("Signal Transformer is NOT using Corrupted Signals, as set by the user");
    }
  }
  else
  {
    INFO("Signal Transformer is not using Corrupted Signals (default option)");
  }

  // Reading file with Side B signals correction to property tree
  if (isOptionSet(fParams.getOptions(), kConstantsFileParamKey))
  {
    boost::property_tree::read_json(getOptionAsString(fParams.getOptions(), kConstantsFileParamKey), fConstansTree);
  }

  // For plotting ToT histograms
  if (isOptionSet(fParams.getOptions(), kToTHistoUpperLimitParamKey))
  {
    fToTHistoUpperLimit = getOptionAsDouble(fParams.getOptions(), kToTHistoUpperLimitParamKey);
  }

  // Getting bool for saving histograms
  if (isOptionSet(fParams.getOptions(), kSaveControlHistosParamKey))
  {
    fSaveControlHistos = getOptionAsBool(fParams.getOptions(), kSaveControlHistosParamKey);
  }

  // Control histograms
  if (fSaveControlHistos)
  {
    initialiseHistograms();
  }
  return true;
}

bool SignalTransformer::exec()
{
  if (auto timeWindow = dynamic_cast<const JPetTimeWindow* const>(fEvent))
  {
    std::vector<JPetMatrixSignal> allSignals;

    for (unsigned int i = 0; i < timeWindow->getNumberOfEvents(); i++)
    {
      auto pmSig = dynamic_cast<const JPetPMSignal&>(timeWindow->operator[](i));

      // Creating one Matrix Signal for each PM signal
      JPetMatrixSignal mtxSig;
      mtxSig.setMatrix(pmSig.getPM().getMatrix());
      mtxSig.addPMSignal(pmSig);
      mtxSig.setTime(SignalTransformerTools::calculateAverageTime(mtxSig, fConstansTree));
      allSignals.push_back(mtxSig);
    }

    saveMatrixSignals(allSignals);
  }
  else
  {
    return false;
  }
  return true;
}

bool SignalTransformer::terminate()
{
  INFO("Signal transforming finished");
  return true;
}

void SignalTransformer::saveMatrixSignals(const std::vector<JPetMatrixSignal>& mtxSigVec)
{
  if (mtxSigVec.size() > 0 && fSaveControlHistos)
  {
    getStatistics().fillHistogram("mtxsig_tslot", mtxSigVec.size());
  }
  for (auto& mtxSig : mtxSigVec)
  {
    fOutputEvents->add<JPetMatrixSignal>(mtxSig);

    if (fSaveControlHistos)
    {
      auto scinID = mtxSig.getMatrix().getScin().getID();
      getStatistics().fillHistogram("mtxsig_multi", mtxSig.getPMSignals().size());
      if (mtxSig.getMatrix().getSide() == JPetMatrix::SideA)
      {
        getStatistics().fillHistogram("mtxsig_scin_sideA", scinID);
        getStatistics().fillHistogram("mtxsig_sideA_tot", scinID, mtxSig.getToT());
      }
      else if (mtxSig.getMatrix().getSide() == JPetMatrix::SideB)
      {
        getStatistics().fillHistogram("mtxsig_scin_sideB", scinID);
        getStatistics().fillHistogram("mtxsig_sideB_tot", scinID, mtxSig.getToT());
      }
    }
  }
}

void SignalTransformer::initialiseHistograms()
{
  auto minScinID = getParamBank().getScins().begin()->first;
  auto maxScinID = getParamBank().getScins().rbegin()->first;

  // MatrixSignal multiplicity
  getStatistics().createHistogramWithAxes(new TH1D("mtxsig_multi", "Multiplicity of matched MatrixSignals", 5, 0.5, 5.5),
                                          "Number of PM Signals in Matrix Signal", "Number of Matrix Signals");

  getStatistics().createHistogramWithAxes(new TH1D("mtxsig_tslot", "Number of Matrix Signals in Time Window", 200, 0.5, 200.5),
                                          "Number of Matrix Signals in Time Window", "Number of Time Windows");

  getStatistics().createHistogramWithAxes(
      new TH1D("mtxsig_scin_sideA", "Number of Matrix Signals per scintillator side A", maxScinID - minScinID + 1, minScinID - 0.5, maxScinID + 0.5),
      "Scin ID", "Number of Matrix Signals");

  getStatistics().createHistogramWithAxes(
      new TH1D("mtxsig_scin_sideB", "Number of Matrix Signals per scintillator side B", maxScinID - minScinID + 1, minScinID - 0.5, maxScinID + 0.5),
      "Scin ID", "Number of Matrix Signals");

  getStatistics().createHistogramWithAxes(new TH2D("mtxsig_sideA_tot", "Matrix Signal ToT - Side A per scintillator", maxScinID - minScinID + 1,
                                                   minScinID - 0.5, maxScinID + 0.5, 200, 0.0, fToTHistoUpperLimit),
                                          "Scin ID", "ToT [ps]");

  getStatistics().createHistogramWithAxes(new TH2D("mtxsig_sideB_tot", "Matrix Signal ToT - Side B per scintillator", maxScinID - minScinID + 1,
                                                   minScinID - 0.5, maxScinID + 0.5, 200, 0.0, fToTHistoUpperLimit),
                                          "Scin ID", "ToT [ps]");
}
