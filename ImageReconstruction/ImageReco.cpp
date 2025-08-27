
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
 *  @file ImageReco.cpp
 */

#include "ImageReco.h"
#include "../CommonTools/EventCategorizerTools.h"
#include <TH3D.h>
#include <TH1I.h>
#include "./JPetOptionsTools/JPetOptionsTools.h"

using namespace jpet_options_tools;

ImageReco::ImageReco(const char *name) : JPetUserTask(name) {}

ImageReco::~ImageReco() {}

bool ImageReco::init()
{
  setUpOptions();
  fOutputEvents = new JPetTimeWindow("JPetEvent");

  getStatistics().createHistogram(new TH3D("hits_pos",
                                           "Reconstructed hit pos",
                                           fNumberOfBinsX, -fXRange, fXRange,
                                           fNumberOfBinsY, -fYRange, fYRange,
                                           fNnumberOfBinsZ, -fZRange, fZRange));
  getStatistics().createHistogram(new TH1I("number_of_events",
                                           "Number of events with n hits",
                                           kNumberOfHitsInEventHisto, 0, kNumberOfHitsInEventHisto));

  getStatistics().createHistogram(new TH1D("annihilation_point_z",
                                           "Annihilation point Z",
                                           fZRange, -fZRange, fZRange));

  getStatistics().createHistogram(new TH1D("annihilation_point_z_cut",
                                           "Annihilation point Z",
                                           fZRange, -fZRange, fZRange));

  return true;
}

bool ImageReco::exec()
{
  if (const auto &timeWindow = dynamic_cast<const JPetTimeWindow *const>(fEvent))
  {
    unsigned int numberOfEventsInTimeWindow = timeWindow->getNumberOfEvents();
    for (unsigned int i = 0; i < numberOfEventsInTimeWindow; i++)
    {
      auto event = dynamic_cast<const JPetEvent &>(timeWindow->operator[](static_cast<int>(i)));
      auto numberOfHits = event.getHits().size();
      getStatistics().getObject<TH1I>("number_of_events")->Fill(numberOfHits);
      if (numberOfHits <= 1)
        continue;
      else
      {
        auto hits = event.getHits();
        for (unsigned int i = 0; i < hits.size() - 1; i++)
        {
          auto firstHit = dynamic_cast<const JPetPhysRecoHit*>(event.getHits().at(i));
          auto secondHit = dynamic_cast<const JPetPhysRecoHit*>(event.getHits().at(i + 1));
          
          auto annhilationPoint = EventCategorizerTools::calculateAnnihilationPoint(firstHit, secondHit);
          getStatistics().getObject<TH1D>("annihilation_point_z")->Fill(annhilationPoint.Z());

          if (annhilationPoint.Z() > -fANNIHILATION_POINT_Z && annhilationPoint.Z() < fANNIHILATION_POINT_Z)
          {
            getStatistics().getObject<TH3D>("hits_pos")->Fill(annhilationPoint.X(), annhilationPoint.Y(), annhilationPoint.Z());
          }    
          else
          {
            getStatistics().getObject<TH1D>("annihilation_point_z_cut")->Fill(annhilationPoint.Z());
          }
        }
      }
    }
  }
  else
  {
    ERROR("Returned event is not TimeWindow");
    return false;
  }
  return true;
}

bool ImageReco::terminate()
{
  return true;
}

void ImageReco::setUpOptions()
{
  auto opts = getOptions();

  if (isOptionSet(opts, kXRangeOn3DHistogramKey))
  {
    fXRange = getOptionAsInt(opts, kXRangeOn3DHistogramKey);
  }

  if (isOptionSet(opts, kYRangeOn3DHistogramKey))
  {
    fYRange = getOptionAsInt(opts, kYRangeOn3DHistogramKey);
  }

  if (isOptionSet(opts, kZRangeOn3DHistogramKey))
  {
    fZRange = getOptionAsInt(opts, kZRangeOn3DHistogramKey);
  }

  if (isOptionSet(opts, kCutOnAnnihilationPointZKey))
  {
    fANNIHILATION_POINT_Z = getOptionAsFloat(opts, kCutOnAnnihilationPointZKey);
  }

  if (isOptionSet(opts, kBinMultiplierKey))
  { //sets-up bin size in root 3d-histogram, root cannot write more then 1073741822 bytes to 1 histogram
    const int kMaxRootFileSize = 1073741822;
    fBinMultiplier = getOptionAsDouble(opts, kBinMultiplierKey);
    if ((std::floor(fBinMultiplier * fXRange) *
         std::floor(fBinMultiplier * fYRange) *
         std::floor(fBinMultiplier * fZRange)) > kMaxRootFileSize)
    {
      fBinMultiplier = 6;
      WARNING("TBufferFile can only write up to 1073741822 bytes, bin multiplier is too big, reseted to 6");
    }
    fNumberOfBinsX = fXRange * fBinMultiplier;
    fNumberOfBinsY = fYRange * fBinMultiplier;
    fNnumberOfBinsZ = fZRange * fBinMultiplier;
  }
}
