/**
 *  @copyright Copyright 2024 The J-PET Framework Authors. All rights reserved.
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may find a copy of the License in the LICENCE file.
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  @file EventCategorizerToolsTest.cpp
 */

#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE EventCategorizerToolsTools

#include "../EventCategorizerTools.h"
#include <boost/test/unit_test.hpp>

auto epsilon = 0.0001;

BOOST_AUTO_TEST_SUITE(EventCategorizerToolsTestSuite)

BOOST_AUTO_TEST_CASE(checkToT_test)
{
  JPetPhysRecoHit hit;
  hit.setToT(57.0);

  BOOST_REQUIRE(EventCategorizerTools::checkToT(&hit, 50.0, 60.0));
  BOOST_REQUIRE(!EventCategorizerTools::checkToT(&hit, 50.0, 55.0));
  BOOST_REQUIRE(!EventCategorizerTools::checkToT(&hit, 59.0, 60.0));
}

BOOST_AUTO_TEST_CASE(checkRelativeAngle_test)
{
  TVector3 pos1(5.0, 0.0, 0.0);
  TVector3 pos2(-5.0, 0.1, 0.0);
  TVector3 pos3(-5.0, 1.0, 0.0);

  BOOST_REQUIRE(EventCategorizerTools::checkRelativeAngles(pos1, pos2, 5.0));
  BOOST_REQUIRE(EventCategorizerTools::checkRelativeAngles(pos1, pos3, 20.0));
  BOOST_REQUIRE(!EventCategorizerTools::checkRelativeAngles(pos1, pos3, 1.0));
}

BOOST_AUTO_TEST_CASE(calculateDistance_test)
{
  JPetBaseHit hit1;
  JPetBaseHit hit2;
  hit1.setPos(1.0, 1.0, 1.0);
  hit2.setPos(-1.0, -1.0, -1.0);

  BOOST_REQUIRE_CLOSE(EventCategorizerTools::calculateDistance(&hit1, &hit2), sqrt(12.0), epsilon);
}

BOOST_AUTO_TEST_CASE(calculatePlaneCenterDistance_test)
{
  JPetBaseHit hit1;
  JPetBaseHit hit2;
  JPetBaseHit hit3;
  hit1.setPos(1.0, -1.0, 0.0);
  hit2.setPos(0.0, 1.0, 0.0);
  hit3.setPos(-1.0, 1.0, 0.0);

  auto distance1 = EventCategorizerTools::calculatePlaneCenterDistance(hit1, hit2, hit3);
  BOOST_REQUIRE_CLOSE(distance1, 0.0, epsilon);

  JPetBaseHit hit4;
  JPetBaseHit hit5;
  JPetBaseHit hit6;

  hit4.setPos(1.0, -1.0, 1.0);
  hit5.setPos(0.0, 1.0, 1.0);
  hit6.setPos(-1.0, 1.0, 1.0);

  auto distance2 = EventCategorizerTools::calculatePlaneCenterDistance(hit4, hit5, hit6);
  BOOST_REQUIRE_CLOSE(distance2, 1.0, epsilon);
}

BOOST_AUTO_TEST_CASE(findIntersection_test)
{
  TVector3 hit1Pos(30, 30, 0);
  TVector3 hit2Pos = hit1Pos;
  hit2Pos.RotateZ(TMath::DegToRad() * 120.0);
  TVector3 hit3Pos = hit1Pos;
  hit3Pos.RotateZ(TMath::DegToRad() * 240.0);

  double t21 = 0;
  double t31 = 0;

  auto result = EventCategorizerTools::findIntersection(hit1Pos, hit2Pos, hit3Pos, t21, t31);
  BOOST_REQUIRE_CLOSE(std::round(result.X() * 1000000) / 1000000, 0.0, epsilon);
  BOOST_REQUIRE_CLOSE(std::round(result.Y() * 1000000) / 1000000, 0.0, epsilon);
  BOOST_REQUIRE_CLOSE(std::round(result.Z() * 1000000) / 1000000, 0.0, epsilon);
}

BOOST_AUTO_TEST_CASE(findIntersectionsOfCircles_test)
{
  TVector3 hit1Pos(0, 0, 0);
  TVector3 hit2Pos(4, 0, 0);
  TVector3 hit3Pos(2, 3, 0);
  double R1 = 3;
  double R2 = 2;
  double R3 = 2;
  double R13 = sqrt(pow(3, 2) + pow(2, 2));
  double R21 = 4;
  double R32 = sqrt(pow(3, 2) + pow(2, 2));

  std::vector<std::vector<double>> result1 = EventCategorizerTools::findIntersectionsOfCircles(hit1Pos, hit2Pos, hit3Pos, R1, R2, R3, R13, R21, R32);

  BOOST_REQUIRE_CLOSE(result1[0][0], R1 * 7 / 8, epsilon);
  BOOST_REQUIRE_CLOSE(result1[0][1], -R1 * sin(TMath::DegToRad() * 28.95502), epsilon);
  BOOST_REQUIRE_CLOSE(result1[1][0], R1 * 7 / 8, epsilon);
  BOOST_REQUIRE_CLOSE(result1[1][1], R1 * sin(TMath::DegToRad() * 28.95502), epsilon);
  BOOST_REQUIRE_CLOSE(result1[2][0], 3.72058, epsilon);
  BOOST_REQUIRE_CLOSE(result1[2][1], 1.9803844, epsilon);
  BOOST_REQUIRE_CLOSE(result1[3][0], 2.2794233, epsilon);
  BOOST_REQUIRE_CLOSE(result1[3][1], 1.0196155, epsilon);
  BOOST_REQUIRE_CLOSE(std::round(result1[4][0] * 1000000) / 1000000, 0.0, epsilon);
  BOOST_REQUIRE_CLOSE(result1[4][1], 3, epsilon);
  BOOST_REQUIRE_CLOSE(result1[5][0], 2.76923, epsilon);
  BOOST_REQUIRE_CLOSE(result1[5][1], 1.1538461, epsilon);
}

BOOST_AUTO_TEST_CASE(findMinimumFromDerivative_test)
{
  std::vector<double> x_vec1 = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  std::vector<double> y_vec1 = {6, 5, 4, 3, 2, 1, 2, 3, 4, 5};

  auto result1 = EventCategorizerTools::findMinimumFromDerivative(x_vec1, y_vec1);
  BOOST_REQUIRE_CLOSE(result1, 6, epsilon);
}

BOOST_AUTO_TEST_CASE(calculatePoint_test)
{
  JPetBaseHit hit1, hit2, hit3;
  TVector3 hit1Pos(30, 30, 0);
  TVector3 hit2Pos = hit1Pos;
  hit2Pos.RotateZ(TMath::DegToRad() * 120.0);
  TVector3 hit3Pos = hit1Pos;
  hit3Pos.RotateZ(TMath::DegToRad() * 240.0);

  hit1.setPos(hit1Pos);
  hit2.setPos(hit2Pos);
  hit3.setPos(hit3Pos);

  hit1.setTime(1000.0);
  hit2.setTime(1000.0);
  hit3.setTime(1000.0);

  auto result1 = EventCategorizerTools::calculateAnnihilationPointByMinimization(hit1, hit2, hit3);
  auto result2pair = EventCategorizerTools::calculateAnnihilationPointAndTimeByTrilateration(hit1, hit2, hit3);

  BOOST_REQUIRE_CLOSE(std::round(result1.X() * 1000000) / 1000000, 0.0, epsilon);
  BOOST_REQUIRE_CLOSE(std::round(result1.Y() * 1000000) / 1000000, 0.0, epsilon);
  BOOST_REQUIRE_CLOSE(std::round(result2pair.second.X() * 1000000) / 1000000, 0.0, epsilon);
  BOOST_REQUIRE_CLOSE(std::round(result2pair.second.Y() * 1000000) / 1000000, 0.0, epsilon);
}

BOOST_AUTO_TEST_SUITE_END()
