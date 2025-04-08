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
 *  @file EventAnalyzer.h
 */

#ifndef EVENTANALYZER_H
#define EVENTANALYZER_H

#include <Hits/JPetMCRecoHit/JPetMCRecoHit.h>
#include <JPetEvent/JPetEvent.h>
#include <JPetRawMCHit/JPetRawMCHit.h>
#include <JPetUserTask/JPetUserTask.h>

class EventAnalyzer : public JPetUserTask
{
public:
  EventAnalyzer(const char* name);
  virtual ~EventAnalyzer();
  virtual bool init() override;
  virtual bool exec() override;
  virtual bool terminate() override;

protected:
  const std::string kSave_oPsOnlyParamKey = "Save_3gamma_oPs_only_bool";
  const std::string k3gMinRelAngleParamKey = "EventCategorizer_3gMinRelativeAngle_double";
  const std::string kSaveControlHistosParamKey = "Save_Control_Histograms_bool";

  bool fSave_oPsOnly = false;
  bool fSaveControlHistos = true;
  double f3gMinRelAngle = 185.0;
  bool fIsMC = false;

  void fillResolutionHistograms(const JPetMCRecoHit* reconstructed_hit, const JPetRawMCHit& mc_hit);
};
#endif /* !EVENTANALYZER_H */
