#!/bin/bash

# Variables defifnitions
userFile="/home/kkacprzak/3work/v11-framework/examples-build/LargeBarrelAnalysis/userParams2.json"
calibFile="/home/kkacprzak/3work/v11-framework/examples-build/LargeBarrelAnalysis/calib.json"
setupFile="/home/kkacprzak/3work/v11-framework/examples-build/LargeBarrelAnalysis/large_barrel_new_format_run15.json"
inputDir="/home/kkacprzak/3work/v11-framework/examples-build/LargeBarrelAnalysis/15_run_lists/"
outputDir="/home/kkacprzak/3work/v11-framework/examples-build/LargeBarrelAnalysis/tof_synchro/"

for i in 1 2 3 4 5 ; do
  echo "   "
  echo "***********"
  echo "Loop $i..."
  echo "***********"

  # Run files from lists in input directory
  for list in $inputDir/*.list ;
  do
    echo " "
    echo "****"
    date_time="`date +%Y-%m-%d_%H:%M:%S`";
    echo $date_time;
    echo "Processing files from list "$list

    while IFS= read -r file
    do
      #echo "$file"
      # Run the analysis of a single file in the background
      /home/kkacprzak/3work/v11-framework/examples-build/LargeBarrelAnalysis/LargeBarrelAnalysis.x -t root -i 15 -l $setupFile -u $userFile -d -f $file -o $outputDir &
    done < "$list"

    # Wait for programs in background to finish
    wait
  done # Finish iterating lists

  # Extracting histograms
  echo "Histograms - processing files from "$outputDir
  for file in $outputDir/*.cat.evt.root ; do
    name="${file%%.cat.evt.root}"
    echo $name
    rootcp $name".cat.evt.root:*/*" $name".C.root"
  done

  echo "HADDing files."

  rm $outputDir/cat.root
  hadd $outputDir/cat.root $outputDir/*.C.root

  echo "Cleaning..."

  rm $outputDir/*.C.root

  # Running ROOT macro
  echo "Run TOF synchronization..."
  root -b -l -q tof_synchro_it.C'("'$outputDir'/cat.root", "'$calibFile'", true, "'$outputDir'", '$i')'

done

echo "All done."
