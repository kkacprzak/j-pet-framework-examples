/**
 *  @copyright Copyright 2025 The J-PET Framework Authors. All rights reserved.
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
 *  @file EventEvaluator.h
 */

#ifndef EVENTEVALUATOR_H
#define EVENTEVALUATOR_H

#include <Hits/JPetMCRecoHit/JPetMCRecoHit.h>
#include <JPetEvent/JPetEvent.h>
#include <JPetRawMCHit/JPetRawMCHit.h>
#include <JPetUserTask/JPetUserTask.h>
#include <TMVA/Reader.h>

class EventEvaluator : public JPetUserTask
{
public:
  EventEvaluator(const char* name);
  virtual ~EventEvaluator();
  virtual bool init() override;
  virtual bool exec() override;
  virtual bool terminate() override;

protected:
  const std::string k3gMinRelAngleParamKey = "EventCategorizer_3gMinRelativeAngle_double";
  const std::string kSaveControlHistosParamKey = "Save_Control_Histograms_bool";
  const std::string kPathToDatasetParamKey = "Path_To_Dataset_std:string";

  bool fSaveControlHistos = true;
  double f3gMinRelAngle = 185.0;
  std::string fPathToDataset = "./";

  std::vector<float> getRelAngles(TVector3 pos1, TVector3 pos2, TVector3 pos3);
  float getScatterTestMeasure(const JPetBaseHit* hit1, const JPetBaseHit* hit2);

  TMVA::Reader* fReader;

  float fRelAng1, fRelAng2, fRelAng3;
  float fTime21, fTime32, fTime31;
  float fTOT1, fTOT2, fTOT3;

  std::map<std::string, float> fMethods;
};
#endif /* !EVENTEVALUATOR_H */
