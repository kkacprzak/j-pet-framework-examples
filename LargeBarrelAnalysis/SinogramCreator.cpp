/**
 *  @copyright Copyright 2017 The J-PET Framework Authors. All rights reserved.
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
 *  @file SinogramCreator.cpp
 */

#include "SinogramCreator.h"
#include <TH2F.h>

SinogramCreator::SinogramCreator(const char* name) : JPetUserTask(name) {}

SinogramCreator::~SinogramCreator() {}

bool SinogramCreator::init()
{
  auto opts = getOptions();
  fOutputEvents = new JPetTimeWindow("JPetEvent");

  getStatistics().createHistogram(new TH2I("reconstuction_histogram",
                                  "reconstuction_histogram",
                                  std::ceil(kReconstructionLayerRadius * 2 * (1.f / kReconstructionDistanceAccuracy)) + 1, -kReconstructionLayerRadius, kReconstructionLayerRadius,
                                  std::ceil((kReconstructionEndAngle - kReconstructionStartAngle) / kReconstructionAngleStep), kReconstructionStartAngle, kReconstructionEndAngle));

  return true;
}

bool SinogramCreator::exec()
{
  assert(kNumberOfScintillatorsInReconstructionLayer % 2 == 0); // number of scintillators always should be parzysta

  int numberOfScintillatorsInHalf = kNumberOfScintillatorsInReconstructionLayer / 2;
  float reconstructionAngleDiff = kReconstructionEndAngle - kReconstructionStartAngle;

  int maxThetaNumber = std::ceil(reconstructionAngleDiff / kReconstructionAngleStep);
  int maxDistanceNumber = std::ceil(kReconstructionLayerRadius * 2 * (1.f / kReconstructionDistanceAccuracy)) + 1;
  if (fSinogram == nullptr) {
    fSinogram = new SinogramResultType(maxDistanceNumber, (std::vector<int>(maxThetaNumber)));
  }
  if (const auto& timeWindow = dynamic_cast<const JPetTimeWindow* const>(fEvent)) {
    unsigned int numberOfEventsInTimeWindow = timeWindow->getNumberOfEvents();
    for (unsigned int i = 0; i < numberOfEventsInTimeWindow; i++) {
      auto event = dynamic_cast<const JPetEvent&>(timeWindow->operator[](static_cast<int>(i)));
      auto hits = event.getHits();
      if (hits.size() == 2) {
        const auto& firstHit = hits[0];
        const auto& secondHit = hits[1];
        if (checkLayer(firstHit) && checkLayer(secondHit)) {
          for (float theta = kReconstructionStartAngle; theta < kReconstructionEndAngle; theta += kReconstructionAngleStep) {
            float x = kReconstructionLayerRadius * std::cos(theta * (M_PI / 180.f)); // calculate x,y positon of line with theta angle from line (0,0) = theta
            float y = kReconstructionLayerRadius * std::sin(theta * (M_PI / 180.f));
            std::pair<float, float> intersectionPoint = SinogramCreatorTools::lineIntersection(std::make_pair(-x, -y), std::make_pair(x, y),
                std::make_pair(firstHit.getPosX(), firstHit.getPosY()), std::make_pair(secondHit.getPosX(), secondHit.getPosY())); //find intersection point
            if (intersectionPoint.first != std::numeric_limits<float>::max() && intersectionPoint.second != std::numeric_limits<float>::max()) { // check is there is intersection point
              float distance = SinogramCreatorTools::length2D(intersectionPoint.first, intersectionPoint.second);
              if (distance >= kReconstructionLayerRadius) // if distance is greather then our max reconstuction layer radius, it cant be placed in sinogram
                continue;
              if (intersectionPoint.first < 0.f)
                distance = -distance;
              int distanceRound = std::floor((kReconstructionLayerRadius / kReconstructionDistanceAccuracy)
                                             + kReconstructionDistanceAccuracy)
                                  + std::floor((distance / kReconstructionDistanceAccuracy) + kReconstructionDistanceAccuracy); //clever way of floating to nearest accuracy digit and change it to int
              int thetaNumber = std::round(theta / kReconstructionAngleStep);                                                   // round because of floating point
              //if (thetaNumber >= maxThetaNumber || distanceRound >= maxDistanceNumber) {
              //std::cout << "Theta: " << thetaNumber << " maxTheta: " << maxThetaNumber << " distanceRound: " << distanceRound << " maxDistanceRound: " << maxDistanceNumber << " distance: " << distance << " ix: " << intersectionPoint.first << " iy: " << intersectionPoint.second << " x1: " << firstHit.getPosX() << " y1: " << firstHit.getPosY() << " x2: " << secondHit.getPosX() << " y2: " << secondHit.getPosY() << std::endl;
              //}
              fSinogram->at(distanceRound).at(thetaNumber)++; // add to sinogram
              getStatistics().getObject<TH2I>("reconstuction_histogram")->Fill(distance, theta); //add to histogram
            }
          }
        }
      }
    }
  } else {
    ERROR("Returned event is not TimeWindow");
    return false;
  }
  return true;
}

bool SinogramCreator::checkLayer(const JPetHit& hit)
{
  return hit.getBarrelSlot().getLayer().getID() == 1;
}

bool SinogramCreator::terminate()
{
  const auto outFile = "sinogram.ppm";
  std::ofstream res(outFile);
  res << "P2" << std::endl;
  res << (*fSinogram)[0].size() << " " << fSinogram->size() << std::endl;
  res << "255" << std::endl;
  for (unsigned int i = 0; i < fSinogram->size(); i++) {
    for (unsigned int j = 0; j < (*fSinogram)[0].size(); j++) {
      res << static_cast<int>((*fSinogram)[i][j]) << " ";
    }
    res << std::endl;
  }
  res.close();
  return true;
}
