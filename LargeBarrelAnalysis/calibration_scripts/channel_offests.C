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
 *  @file channel_offests.C
 *
 *  @brief Script for reading histograms with channel offsets and producing calibraiton json file
 *
 *  This script uses histograms, that are produced by task SignalTransformer.
 *  Channels that belong to SiPM in the same mattix are synchronized to channel
 *  on THR1 of SiPM with matrix position 1.
 *
 *  Basic usage:
 *  root> .L channel_offsets.C
 *  root> channel_offests("file_with_calib_histos.root")
 *  -- this will produce file "calibration_constants.json" with the results. If the
 *  file exists, the result of this calibration will be appended to the existing tree.
 */

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <TCanvas.h>
#include <TDirectory.h>
#include <TFile.h>
#include <TGraph.h>
#include <TH1D.h>
#include <TH2D.h>
#include <TLine.h>
#include <TMath.h>

#include <fstream>
#include <iostream>
#include <vector>

namespace bpt = boost::property_tree;

void channel_offsets(std::string fileName, std::string calibJSONFileName = "calibration_constants.json", bool saveResult = false,
                     std::string resultDir = "./", int minPMID = 1, int maxPMID = 384)
{
  TFile* fileTHRSynchro = new TFile(fileName.c_str(), "READ");

  bpt::ptree tree;
  ifstream file(calibJSONFileName.c_str());
  if (file.good())
  {
    bpt::read_json(calibJSONFileName, tree);
  }

  if (fileTHRSynchro->IsOpen())
  {
    // Iterating over 4 thresholds
    for (int thr = 2; thr <= 4; thr++)
    {
      TH2D* channelOffsets = dynamic_cast<TH2D*>(fileTHRSynchro->Get(Form("lead_thr1_thr%d_diff_pm", thr)));

      for (int pmID = minPMID; pmID <= maxPMID; ++pmID)
      {
        TH1D* offsetHist = channelOffsets->ProjectionY(Form("offset_pm%d_thr%d", pmID, thr), pmID - minPMID + 1, pmID - minPMID + 1);
        // if (offsetHist->GetEntries() < 100)
        // {
        //   continue;
        // }

        // Offset is a time indicated by bin with highest number of counts
        double offset = offsetHist->GetBinCenter(offsetHist->GetMaximumBin());
        offset += tree.get("pm_thr_offsets." + to_string(pmID) + "." + to_string(thr), 0.0);
        tree.put("pm_thr_offsets." + to_string(pmID) + "." + to_string(thr), offset);

        if (offsetHist->GetEntries() < 500)
        {
          offset = 0.0;
        }

        if (saveResult)
        {
          auto name = Form("offset_result_pm%d_thr%d", pmID, thr);

          TCanvas* can = new TCanvas(name, name, 900, 720);
          offsetHist->Draw();

          TLine* line = new TLine(offset, offsetHist->GetMinimum(), offset, offsetHist->GetMaximum());
          line->SetLineWidth(2);
          line->SetLineColor(kRed);
          line->Draw("same");

          can->SaveAs(Form("%s/offset_pm%d_thr%d.png", resultDir.c_str(), pmID, thr));
        }
      }
    }
  }

  // Saving tree into json file
  bpt::write_json(calibJSONFileName, tree);
}
