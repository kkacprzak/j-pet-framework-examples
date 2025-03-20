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
 *  @file SignalTransformer.h
 */

#ifndef SIGNALTRANSFORMER_H
#define SIGNALTRANSFORMER_H

#include "JPetUserTask/JPetUserTask.h"
#include "Signals/JPetMatrixSignal/JPetMatrixSignal.h"
#include <boost/property_tree/ptree.hpp>

class JPetWriter;

/**
 * @brief User Task: method rewriting Raw Signals to Reco and Phys Signals.
 *
 * Task rewrites Raw Signals to Reco Signals and Physical Signals, saving JPetPhysSignal.
 * Only time of the signal is set, the rest of available fields are set to -1,
 * also using corrupted signal, if indicated by user.
 */
class SignalTransformer : public JPetUserTask
{
public:
  SignalTransformer(const char* name);
  virtual ~SignalTransformer();
  virtual bool init() override;
  virtual bool exec() override;
  virtual bool terminate() override;

protected:
  void initialiseHistograms();
  const std::string kUseCorruptedSignalsParamKey = "SignalTransformer_UseCorruptedSignals_bool";
  const std::string kToTHistoUpperLimitParamKey = "ToTHisto_UpperLimit_double";
  const std::string kSaveControlHistosParamKey = "Save_Control_Histograms_bool";
  const std::string kConstantsFileParamKey = "ConstantsFile_std::string";
  void saveMatrixSignals(const std::vector<JPetMatrixSignal>& mtxSigVec);
  boost::property_tree::ptree fConstansTree;
  double fToTHistoUpperLimit = 200000.0;
  bool fUseCorruptedSignals = false;
  bool fSaveControlHistos = true;
};
#endif /* !SIGNALTRANSFORMER_H */
