//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// LSDSpatialCSVReader.hpp
// Land Surface Dynamics SpatialCSVReader
//
// An object within the University
//  of Edinburgh Land Surface Dynamics group topographic toolbox
//  for reading csv data. The data needs to have latitude and longitude 
//  in WGS84 coordinates.
//
// Developed by:
//  Simon M. Mudd
//  Martin D. Hurst
//  David T. Milodowski
//  Stuart W.D. Grieve
//  Declan A. Valters
//  Fiona Clubb
//
// Copyright (C) 2017 Simon M. Mudd 2017
//
// Developer can be contacted by simon.m.mudd _at_ ed.ac.uk
//
//    Simon Mudd                                                    
//    University of Edinburgh
//    School of GeoSciences
//    Drummond Street
//    Edinburgh, EH8 9XP
//    Scotland
//    United Kingdom
//
// This program is free software;
// you can redistribute it and/or modify it under the terms of the
// GNU General Public License as published by the Free Software Foundation;
// either version 2 of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY;
// without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
//
// You should have received a copy of the
// GNU General Public License along with this program;
// if not, write to:
// Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor,
// Boston, MA 02110-1301
// USA
//
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
#include <fstream>
#include <cmath>
#include <iostream>
#include <map>
#include <string>
#include <ctype.h>
#include <sstream>
#include <algorithm> 
#include <vector>
#include "LSDStatsTools.hpp"
#include "LSDShapeTools.hpp"
#include "LSDCosmoData.hpp"
#include "LSDRaster.hpp"
#include "LSDFlowInfo.hpp"
#include "LSDJunctionNetwork.hpp"
#include "LSDBasin.hpp"
#include "LSDSpatialCSVReader.hpp"
#include "LSDRasterInfo.hpp"
#include "TNT/tnt.h"
using namespace std;
using namespace TNT;

#ifndef LSDSpatialCSVReader_CPP
#define LSDSpatialCSVReader_CPP

void LSDSpatialCSVReader::create(LSDRasterInfo& ThisRasterInfo, string csv_fname)
{
  NRows = ThisRasterInfo.get_NRows();
  NCols = ThisRasterInfo.get_NCols();
  XMinimum = ThisRasterInfo.get_XMinimum();
  YMinimum = ThisRasterInfo.get_YMinimum();
  DataResolution = ThisRasterInfo.get_DataResolution();
  NoDataValue = ThisRasterInfo.get_NoDataValue();
  GeoReferencingStrings = ThisRasterInfo.get_GeoReferencingStrings();
  
  load_csv_data(csv_fname);
  
}

void LSDSpatialCSVReader::create(LSDRaster& ThisRaster, string csv_fname)
{
  NRows = ThisRaster.get_NRows();
  NCols = ThisRaster.get_NCols();
  XMinimum = ThisRaster.get_XMinimum();
  YMinimum = ThisRaster.get_YMinimum();
  DataResolution = ThisRaster.get_DataResolution();
  NoDataValue = ThisRaster.get_NoDataValue();
  GeoReferencingStrings = ThisRaster.get_GeoReferencingStrings();
  
  load_csv_data(csv_fname);
}


//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//
// This loads a csv file 
//
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
void LSDSpatialCSVReader::load_csv_data(string filename)
{
  // make sure the filename works
  ifstream ifs(filename.c_str());
  if( ifs.fail() )
  {
    cout << "\nFATAL ERROR: Trying to load csv cosmo data file, but the file" << filename
         << "doesn't exist; LINE 245 LSDCosmoData" << endl;
    exit(EXIT_FAILURE);
  }
  else
  {
    cout << "I have opened the csv file." << endl;
  }
  
  // Initiate the data map
  map<string, vector<string> > temp_data_map;

  // initiate the string to hold the file
  string line_from_file;
  vector<string> empty_string_vec;
  vector<string> this_string_vec;
  string temp_string;
  
  vector<double> temp_longitude;
  vector<double> temp_latitude;
  
  // get the headers from the first line
  getline(ifs, line_from_file);
  
  // reset the string vec
  this_string_vec = empty_string_vec;
    
  // create a stringstream
  stringstream ss(line_from_file);
  ss.precision(9);

  
  while( ss.good() )
  {
    string substr;
    getline( ss, substr, ',' );
      
    // remove the spaces
    substr.erase(remove_if(substr.begin(), substr.end(), ::isspace), substr.end());
      
    // remove control characters
    substr.erase(remove_if(substr.begin(), substr.end(), ::iscntrl), substr.end());
      
    // add the string to the string vec
    this_string_vec.push_back( substr );
  }
  // now check the data map
  int n_headers = int(this_string_vec.size());
  vector<string> header_vector = this_string_vec;
  int latitude_index = -9999;
  int longitude_index = -9999;
  for (int i = 0; i<n_headers; i++)
  {
    cout << "This header is: " << this_string_vec[i] << endl;
    if (this_string_vec[i]== "latitude")
    {
      latitude_index = i;
      cout << "The latitude index is: " << latitude_index << endl;
    
    }
    else if (this_string_vec[i] == "longitude")
    {
      longitude_index = i;
      cout << "The longitude index is: " << longitude_index << endl;
    }
    else
    {
      temp_data_map[header_vector[i]] = empty_string_vec;
    }
  }
  
  
  // now loop through the rest of the lines, getting the data. 
  while( getline(ifs, line_from_file))
  {
    cout << "Getting line, it is: " << line_from_file << endl;
    // reset the string vec
    this_string_vec = empty_string_vec;
    
    // create a stringstream
    stringstream ss(line_from_file);
    
    while( ss.good() )
    {
      string substr;
      getline( ss, substr, ',' );
      
      // remove the spaces
      substr.erase(remove_if(substr.begin(), substr.end(), ::isspace), substr.end());
      
      // remove control characters
      substr.erase(remove_if(substr.begin(), substr.end(), ::iscntrl), substr.end());
      
      // add the string to the string vec
      this_string_vec.push_back( substr );
    }
    
    //cout << "Yoyoma! size of the string vec: " <<  this_string_vec.size() << endl;
    if ( int(this_string_vec.size()) <= 0)
    {
      cout << "Hey there, I am trying to load your csv data but you seem not to have" << endl;
      cout << "enough columns in your file. I am ignoring a line" << endl;
    }
    else
    {
      int n_cols = int(this_string_vec.size());
      cout << "N cols is: " << n_cols << endl;
      for (int i = 0; i<n_cols; i++)
      {
        if (i == latitude_index)
        {
          temp_latitude.push_back( atof(this_string_vec[i].c_str() ) );
        }
        else if (i == longitude_index)
        {
          temp_longitude.push_back( atof(this_string_vec[i].c_str() ) );
        
        }
        else
        {
          temp_data_map[header_vector[i]].push_back(this_string_vec[i]);
        }
      
      }
    }
  
  }
  latitude = temp_latitude;
  longitude = temp_longitude;
  data_map = temp_data_map;
}


void LSDSpatialCSVReader::print_lat_long_to_screen()
{
  int N_data = int(latitude.size());
  cout << "latitude,longitude"<< endl;
  cout.precision(9);
  for (int i = 0; i< N_data; i++)
  {
    cout << latitude[i] << "," << longitude[i] << endl;
  }

}


#endif