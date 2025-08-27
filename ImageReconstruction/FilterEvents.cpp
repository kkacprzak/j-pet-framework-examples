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
 *  @file FilterEvents.cpp
 */

#include "FilterEvents.h"
#include <TH3D.h>
#include <TH1I.h>
#include "JPetOptionsTools/JPetOptionsTools.h"
#include "JPetEvent/JPetEvent.h"

using namespace jpet_options_tools;
using namespace std;

FilterEvents::FilterEvents(const char* name) : JPetUserTask(name) {}

FilterEvents::~FilterEvents() {}

bool FilterEvents::init()
{
  setUpOptions();
  fOutputEvents = new JPetTimeWindow("JPetEvent");

  getStatistics().createHistogram(new TH1I("number_of_events", "Number of events with n hits",
                                  kNumberOfHitsInEventHisto, 0, kNumberOfHitsInEventHisto));
  getStatistics().createHistogram(new TH1I("number_of_hits_filtered_by_condition",
                                  "Number of hits filtered by condition",
                                  kNumberOfConditions, 0.5, kNumberOfConditions + 0.5));

  vector<pair<unsigned int, string>> binLabels = {
    make_pair(1, "Cut on Z"), make_pair(2, "Cut on LOR distance"), make_pair(3, "Cut on delta angle"), 
    make_pair(4, "Cut on first hit TOT"), make_pair(5, "Cut on second hit TOT"), make_pair(6, "Cut on annihilation point Z")
  };
  getStatistics().setHistogramBinLabel("number_of_hits_filtered_by_condition", getStatistics().AxisLabel::kXaxis, binLabels);

  return true;
}

bool FilterEvents::exec()
{
  if (const auto& timeWindow = dynamic_cast<const JPetTimeWindow* const>(fEvent)) {
    unsigned int numberOfEventsInTimeWindow = timeWindow->getNumberOfEvents();
    for (unsigned int i = 0; i < numberOfEventsInTimeWindow; i++) {
      auto event = dynamic_cast<const JPetEvent&>(timeWindow->operator[](static_cast<int>(i)));
      auto numberOfHits = event.getHits().size();
      if (numberOfHits <= 1)
        continue;
      else {
        auto hits = event.getHits();
        for (unsigned int i = 0; i < hits.size() - 1; i++) 
        {
          auto firstHit = dynamic_cast<const JPetPhysRecoHit*>(event.getHits().at(i));
          auto secondHit = dynamic_cast<const JPetPhysRecoHit*>(event.getHits().at(i + 1));
          if (!checkConditions(firstHit, secondHit))
            continue;
          fOutputEvents->add<JPetEvent>(event);
        }
      }
    }
  } else {
    ERROR("Returned event is not TimeWindow");
    return false;
  }
  return true;
}

bool FilterEvents::terminate()
{
  return true;
}

bool FilterEvents::checkConditions(const JPetPhysRecoHit* first, const JPetPhysRecoHit* second)
{
  if (!cutOnZ(first, second)) {
    getStatistics().fillHistogram("number_of_hits_filtered_by_condition", 1);
    return false;
  }
  if (!cutOnLORDistanceFromCenter(first, second)) {
    getStatistics().fillHistogram("number_of_hits_filtered_by_condition", 2);
    return false;
  }
  if (angleDelta(first, second) < fAngleDeltaMinValue) {
    getStatistics().fillHistogram("number_of_hits_filtered_by_condition", 3);
    return false;
  }

  double totOfFirstHit = first->getToT() / 1000.0; // [ns]
  if (totOfFirstHit < fTOTMinValueInNs || totOfFirstHit > fTOTMaxValueInNs) {
    getStatistics().fillHistogram("number_of_hits_filtered_by_condition", 4);
    return false;
  }

  double totOfSecondHit = second->getToT() / 1000.0; // [ns]
  if (totOfSecondHit < fTOTMinValueInNs || totOfSecondHit > fTOTMaxValueInNs) {
    getStatistics().fillHistogram("number_of_hits_filtered_by_condition", 5);
    return false;
  }

  return true;
}

bool FilterEvents::cutOnZ(const JPetPhysRecoHit* first, const JPetPhysRecoHit* second)
{
  return (std::fabs(first->getPosZ()) < fCutOnZValue) && (fabs(second->getPosZ()) < fCutOnZValue);
}

bool FilterEvents::cutOnLORDistanceFromCenter(const JPetPhysRecoHit* first, const JPetPhysRecoHit* second)
{
  double x_a = first->getPosX();
  double x_b = second->getPosX();

  double y_a = first->getPosY();
  double y_b = second->getPosY();

  double a = (y_a - y_b) / (x_a - x_b);
  double c = y_a - ((y_a - y_b) / (x_a - x_b)) * x_a;
  return (std::fabs(c) / std::sqrt(a * a + 1)) < fCutOnLORDistanceFromCenter; //b is 1 and b*b is 1
}

float FilterEvents::angleDelta(const JPetPhysRecoHit* first, const JPetPhysRecoHit* second)
{
  float delta = fabs(first->getScin().getSlot().getTheta() - second->getScin().getSlot().getTheta());
  return std::min(delta, (float)360 - delta);
}

void FilterEvents::setUpOptions()
{
  auto opts = getOptions();

  if (isOptionSet(opts, kCutOnZValueKey)) {
    fCutOnZValue = getOptionAsFloat(opts, kCutOnZValueKey);
  }
  if (isOptionSet(opts, kCutOnLORDistanceKey)) {
    fCutOnLORDistanceFromCenter = getOptionAsFloat(opts, kCutOnLORDistanceKey);
  }
  if (isOptionSet(opts, kCutOnTOTMinValueKey)) {
    fTOTMinValueInNs = getOptionAsFloat(opts, kCutOnTOTMinValueKey);
  }
  if (isOptionSet(opts, kCutOnTOTMaxValueKey)) {
    fTOTMaxValueInNs = getOptionAsFloat(opts, kCutOnTOTMaxValueKey);
  }
  if (isOptionSet(opts, kCutOnAngleDeltaMinValueKey)) {
    fAngleDeltaMinValue = getOptionAsFloat(opts, kCutOnAngleDeltaMinValueKey);
  }
}
