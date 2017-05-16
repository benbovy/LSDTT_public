//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// LSDChiTools
// Land Surface Dynamics ChiTools object
//
// The header of the ChiTools object within the University
//  of Edinburgh Land Surface Dynamics group topographic toolbox
//  for performing various analyses in chi space
//
//
// Developed by:
//  Simon M. Mudd
//  Martin D. Hurst
//  David T. Milodowski
//  Stuart W.D. Grieve
//  Declan A. Valters
//  Fiona Clubb
//
// Copyright (C) 2016 Simon M. Mudd 2016
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


//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// LSDChiTools.cpp
// LSDChiTools object
// LSD stands for Land Surface Dynamics
//
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// This object is written by
// Simon M. Mudd, University of Edinburgh
//
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//-----------------------------------------------------------------
//DOCUMENTATION URL: http://www.geos.ed.ac.uk/~s0675405/LSD_Docs/
//-----------------------------------------------------------------



#ifndef LSDChiTools_HPP
#define LSDChiTools_HPP

#include <vector>
#include <string>
#include <fstream>
#include <algorithm>
#include "TNT/tnt.h"
#include "LSDFlowInfo.hpp"
#include "LSDRaster.hpp"
#include "LSDChannel.hpp"
#include "LSDJunctionNetwork.hpp"
#include "LSDIndexChannel.hpp"
#include "LSDStatsTools.hpp"
#include "LSDShapeTools.hpp"
using namespace std;
using namespace TNT;


/// @brief This object packages a number of tools for chi analysis
class LSDChiTools
{
  public:

    /// @brief Create an LSDChiTools from a raster.
    /// @param ThisRaster An LSDRaster object
    /// @author SMM
    /// @date 24/05/2016
    LSDChiTools(LSDRaster& ThisRaster)  { create(ThisRaster); }

    /// @brief Create an LSDChiTools from a raster.
    /// @param ThisRaster An LSDIndexRaster object
    /// @author SMM
    /// @date 24/05/2016
    LSDChiTools(LSDIndexRaster& ThisRaster)  { create(ThisRaster); }

    /// @brief Create an LSDChiTools from a LSDFlowInfo object.
    /// @param ThisFI An LSDFlowInfo object
    /// @author SMM
    /// @date 24/05/2016
    LSDChiTools(LSDFlowInfo& ThisFI)  { create(ThisFI); }

    /// @brief Create an LSDChiTools from a LSDJunctionNetwork.
    /// @param ThisJN An LSDJunctionNetwork object
    /// @author SMM
    /// @date 24/05/2016
    LSDChiTools(LSDJunctionNetwork& ThisJN)  { create(ThisJN); }

    /// @brief This resets all the data maps
    /// @author SMM
    /// :date 02/06/2016
    void reset_data_maps();

    /// @brief this gets the x and y location of a node at row and column
    /// @param row the row of the node
    /// @param col the column of the node
    /// @param x_loc the x location (Northing) of the node
    /// @param y_loc the y location (Easting) of the node
    /// @author SMM
    /// @date 22/12/2014
    void get_x_and_y_locations(int row, int col, double& x_loc, double& y_loc);

    /// @brief this gets the x and y location of a node at row and column
    /// @param row the row of the node
    /// @param col the column of the node
    /// @param x_loc the x location (Northing) of the node
    /// @param y_loc the y location (Easting) of the node
    /// @author SMM
    /// @date 22/12/2014
    void get_x_and_y_locations(int row, int col, float& x_loc, float& y_loc);

    /// @brief a function to get the lat and long of a node in the raster
    /// @detail Assumes WGS84 ellipsiod
    /// @param row the row of the node
    /// @param col the col of the node
    /// @param lat the latitude of the node (in decimal degrees, replaced by function)
    ///  Note: this is a double, because a float does not have sufficient precision
    ///  relative to a UTM location (which is in metres)
    /// @param long the longitude of the node (in decimal degrees, replaced by function)
    ///  Note: this is a double, because a float does not have sufficient precision
    ///  relative to a UTM location (which is in metres)
    /// @param Converter a converter object (from LSDShapeTools)
    /// @author SMM
    /// @date 24/05/2015
    void get_lat_and_long_locations(int row, int col, double& lat,
                  double& longitude, LSDCoordinateConverterLLandUTM Converter);

    /// @brief this function gets the UTM_zone and a boolean that is true if
    /// the map is in the northern hemisphere
    /// @param UTM_zone the UTM zone. Replaced in function.
    /// @param is_North a boolean that is true if the DEM is in the northern hemisphere.
    ///  replaced in function
    /// @author SMM
    /// @date 22/12/2014
    void get_UTM_information(int& UTM_zone, bool& is_North);


    /// @brief This function makes a chi map and prints to a csv file
    /// @detail the lat and long coordinates in the csv are in WGS84
    /// @param FlowInfo an LSDFlowInfo object
    /// @param filename The string filename including path and extension
    /// @param A_0 the A_0 parameter
    /// @param m_over_n the m/n ratio
    /// @param area_threshold the threshold over which to print chi
    /// @author SMM
    /// @date 24/05/2016
    void chi_map_to_csv(LSDFlowInfo& FlowInfo, string filename,
                        float A_0, float m_over_n, float area_threshold);

    /// @brief This function takes a raster prints to a csv file
    /// @detail the lat and long coordinates in the csv are in WGS84
    /// @param FlowInfo an LSDFlowInfo object
    /// @param filename The string filename including path and extension
    /// @param chi_coord the raster of the chi coordinate (printed elsewhere)
    /// @author SMM
    /// @date 03/06/2016
    void chi_map_to_csv(LSDFlowInfo& FlowInfo, string chi_map_fname, LSDRaster& chi_coord);

    /// @brief This function takes a raster prints to a csv file. Includes the junction number in the file
    /// @detail the lat and long coordinates in the csv are in WGS84
    /// @param FlowInfo an LSDFlowInfo object
    /// @param filename The string filename including path and extension
    /// @param chi_coord the raster of the chi coordinate (printed elsewhere)
    /// @param basin_raster A raster with the basin numbers (calculated elsewhere)
    /// @author SMM
    /// @date 31/01/2017
    void chi_map_to_csv(LSDFlowInfo& FlowInfo, string chi_map_fname, LSDRaster& chi_coord, LSDIndexRaster& basin_raster);

    /// @brief This function is used to tag channels with a segment number
    ///  It decides on segments if the M_Chi value has changed so should only be used
    ///  with chi networks that have used a skip of 0 and a monte carlo itertions of 1
    ///  This data is used by other routines to look at the spatial distribution of
    ///  hillslope-channel coupling.
    /// @detail WARNING: ONLY use if you have segmented with skip 0 and iterations 1. Otherwise
    ///  you will get a new segment for every channel pixel
    /// @param FlowInfo an LSDFlowInfo object
    /// @author SMM
    /// @date 4/02/2017
    void segment_counter(LSDFlowInfo& FlowInfo);

    /// @brief This function calculates the fitted elevations: It uses m_chi and b_chi
    ///  data to get the fitted elevation of the channel points.
    /// @param FlowInfo an LSDFlowInfo object
    /// @author SMM
    /// @date 4/02/2017
    void segment_counter_knickpoint(LSDFlowInfo& FlowInfo, float threshold_knickpoint, float threshold_knickpoint_length);

    /// @brief Development function based on segment_counter to help
    ///  knickpoint detection. More description will be added when it will be
    ///  functional.
    /// @param FlowInfo an LSDFlowInfo object
    /// @param float threshold_knickpoint the knickpoints detection threshold
    /// @author BG
    /// @date 10/02/2017
    void calculate_segmented_elevation(LSDFlowInfo& FlowInfo);

    /// @brief This returns an maximum liklihood estiamtor by comparing
    ///  a channel (with a particular source number) against a reference channel
    /// @param FlowInfo an LSDFlowInfo object
    /// @param reference_channel the source key of the reference channel
    /// @param test_channel the source key of the test channel
    /// @return The maximum likelihood estimator
    /// @author SMM
    /// @date 04/05/2017
    float test_segment_collinearity(LSDFlowInfo& FlowInfo, int reference_channel, int test_channel);

    /// @brief This computes a collinearity metric for all combinations of 
    ///  channels
    /// @detail It takes all the combinations of sources and gets the goodness of fit between each pair
    ///  of sources. 
    /// @param FlowInfo an LSDFlowInfo object
    /// @param only_use_mainstem_as_reference True if you only want to use the mainstem
    /// @param reference_source integer vector replaced in function that has the reference vector for each comparison 
    /// @param test_source integer vector replaced in function that has the test vector for each comparison 
    /// @param MLE_values the MLE for each comparison. Replaced in function.
    /// @param RMSE_values the RMSE for each comparison (i.e. between source 0 1, 0 2, 0 3, etc.). Replaced in function. 
    /// @author SMM
    /// @date 08/05/2017
    float test_all_segment_collinearity(LSDFlowInfo& FlowInfo, bool only_use_mainstem_as_reference, 
                                        vector<int>& reference_source, vector<int>& test_source, 
                                        vector<float>& MLE_values, vector<float> RMSE_values);

    /// @brief This wraps the collinearity tester, looping through different m over n
    ///  values and calculating goodness of fit statistics. 
    /// @param FlowInfo an LSDFlowInfo object
    /// @param source_nodes a vector containing the sorted sorce nodes (by flow distance)
    /// @param outlet_nodes a vector continaing the outlet nodes
    /// @param Elevation an LSDRaster containing elevation info
    /// @param DistanceFromOutlet an LSDRaster with the flow distance
    /// @param DrainageArea an LSDRaster with the drainage area
    /// @param start_movern the starting m/n ration
    /// @param delta_movern the change in m/n 
    /// @param n_novern the number of m/n values to use
    /// @param only_use_mainstem_as_reference a boolean, if true only compare channels to mainstem .
    /// @author SMM
    /// @date 16/05/2017
    void calcualte_goodness_of_fit_collinearity_fxn_movern(LSDFlowInfo& FlowInfo, 
                        vector<int> source_nodes, vector<int> outlet_nodes, 
                        LSDRaster& Elevation, LSDRaster& DistanceFromOutlet, 
                        LSDRaster& DrainageArea, 
                        float start_movern, float delta_movern, int n_movern, 
                        bool only_use_mainstem_as_reference);




    /// @brief This gets the node index (the reference into LSDFlowInfo) of a source
    ///  based on a source key
    /// @param source_key the source key of the reference channel
    /// @return the node index of the source node
    /// @author SMM
    /// @date 06/05/2017
    int get_source_from_source_key(int source_key);

    /// @brief This gets the index into the node_sequence vector of the first
    ///  node in a channel identified by its source key
    /// @param source_key the source key of the reference channel
    /// @return the index into the node_sequence vector of the source node of the channel with source_key
    /// @author SMM
    /// @date 04/05/2017
    int get_starting_node_of_source(int source_key);
    
    /// @brief Gets the number of channels in the DEM
    /// @return number of channels
    /// @author SMM
    /// @date 05/05/2017
    int get_number_of_channels();

    /// @brief This takes a source key and a flow info object and overwrites vectors
    ///  containing chi and elevation data from a channel tagged by a source
    ///  key. The idea is to use this in the MLE comparison between two channels
    ///  to check for collinearity
    /// @param FlowInfo and LSDFlowInfo object
    /// @param source_key The key of the source
    /// @param chi_data A vector holding chi data of the channel. Will be overwritten
    /// @param elevation_data A vector holding elevation data of the channel. Will be overwritten
    /// @author SMM
    /// @date 06/05/2017
    void get_chi_elevation_data_of_channel(LSDFlowInfo& FlowInfo, int source_key, 
                                vector<float>& chi_data, vector<float>& elevation_data);

    /// @brief This takes the chi locations of a tributarry vector and then uses
    ///  linear interpolation to determine the elevation on a reference channel
    ///  at those chi values
    /// @param reference_chi the chi coordiantes of the reference channel
    /// @param reference_elevation the elevations on the reference channel
    /// @param trib_chi the chi coordiantes of the tributary channel
    /// @param trib_elevation the elevations on the tributary channel
    /// @return A vector of the elevations on the chi locations of the tributary channel
    /// @author SMM
    /// @date 07/05/2017
    vector<float> project_data_onto_reference_channel(vector<float>& reference_chi,
                                 vector<float>& reference_elevation, vector<float>& trib_chi,
                                 vector<float>& trib_elevation);

    /// @brief This function burns the chi coordinate (and area, flow distance and elevation)
    ///  onto the data maps in the chitools object. It does not do any segmentation.
    /// @detail The purpose of this function is to get the chi coordinate without
    ///   calculating m_chi or segmenting, and is used for m/n calculations. Can
    ///   also be used for maps of chi coordinate
    /// @param FlowInfo an LSDFlowInfo object
    /// @param source_nodes a vector containing the sorted sorce nodes (by flow distance)
    /// @param outlet_nodes a vector continaing the outlet nodes
    /// @param Elevation an LSDRaster containing elevation info
    /// @param DistanceFromOutlet an LSDRaster with the flow distance
    /// @param DrainageArea an LSDRaster with the drainage area
    /// @author SMM
    /// @date 16/05/2017
    void chi_map_automator_chi_only(LSDFlowInfo& FlowInfo,
                                    vector<int> source_nodes,
                                    vector<int> outlet_nodes,
                                    LSDRaster& Elevation, LSDRaster& FlowDistance,
                                    LSDRaster& DrainageArea, LSDRaster& chi_coordinate);

    /// @brief This function maps out the chi steepness and other channel
    ///  metrics in chi space from all the sources supplied in the
    ///  source_nodes vector. The source and outlet nodes vector is
    ///  generated by LSDJunctionNetwork.get_overlapping_channels
    /// @detail Takes vector so source and outlet nodes and performs the segment
    ///  fitting routine on them
    /// @param FlowInfo an LSDFlowInfo object
    /// @param source_nodes a vector containing the sorted sorce nodes (by flow distance)
    /// @param outlet_nodes a vector continaing the outlet nodes
    /// @param Elevation an LSDRaster containing elevation info
    /// @param DistanceFromOutlet an LSDRaster with the flow distance
    /// @param DrainageArea an LSDRaster with the drainage area
    /// @param target_nodes int the target number of nodes in a break
    /// @param n_iterations  int the number of iterations
    /// @param target_skip int the mean skipping value
    /// @param minimum_segment_length How many nodes the mimimum segment will have.
    /// @param sigma Standard deviation of error on elevation data
    /// @author SMM
    /// @date 23/05/2016
    void chi_map_automator(LSDFlowInfo& FlowInfo, vector<int> source_nodes,
                           vector<int> outlet_nodes, LSDRaster& Elevation, LSDRaster& FlowDistance,
                           LSDRaster& DrainageArea, LSDRaster& chi_coordinate,
                           int target_nodes, int n_iterations, int skip,
                           int minimum_segment_length, float sigma);

    /// @brief This function maps out the chi steepness and other channel
    ///  metrics in chi space from all the sources supplied in the
    ///  source_nodes vector. The source and outlet nodes vector is
    ///  generated by LSDJunctionNetwork.get_overlapping_channels
    /// @detail This is simpler than the above function: it simply performs
    ///   a linear regression ofve a fixed number of data points: no effort is
    ///   made to segment the data. It is thus much closer to a k_sn plot
    /// @param FlowInfo an LSDFlowInfo object
    /// @param source_nodes a vector continaing the sorted sorce nodes (by flow distance)
    /// @param outlet_nodes a vector continaing the outlet nodes
    /// @param Elevation an LSDRaster containing elevation info
    /// @param DistanceFromOutlet an LSDRaster with the flow distance
    /// @param DrainageArea an LSDRaster with the drainage area
    /// @param regression_nodes the number of nodes in each segment over which
    ///   to perform a linear regression. This number should be odd to it has
    ///   a clear midpoint
    /// @author SMM
    /// @date 02/06/2016
    void chi_map_automator_rudimentary(LSDFlowInfo& FlowInfo, vector<int> source_nodes, vector<int> outlet_nodes,
                                    LSDRaster& Elevation, LSDRaster& FlowDistance,
                                    LSDRaster& DrainageArea, LSDRaster& chi_coordinate,
                                    int regression_nodes);

    /// @brief This returns an LSDIndexRaster with basins numbered by outlet junction
    /// @param FlowInfo an LSDFlowInfo object
    /// @param JN the junction network object
    /// @param Junctions The baselevel junctions to be printed
    /// @return The basin raster
    /// @author SMM
    /// @date 19/01/2017
    LSDIndexRaster get_basin_raster(LSDFlowInfo& FlowInfo, LSDJunctionNetwork& JunctionNetwork,
                               vector<int> Juntions);


    /// @brief This prints a csv file that has the locations of the sources and their keys
    ///  latitude,longitude,source_node, source_key
    /// @param FlowInfo an LSDFlowInfo object
    /// @param filename The name of the filename to print to (should have full
    ///   path and the extension .csv
    /// @author SMM
    /// @date 16/01/2017
    void print_source_keys(LSDFlowInfo& FlowInfo, string filename);

    /// @brief This prints a csv file that has the locations of the baselevels and their keys
    ///  latitude,longitude,baselevel_junctione, baselevel_key
    /// @param FlowInfo an LSDFlowInfo object
    /// @param JN the junction network object
    /// @param filename The name of the filename to print to (should have full
    ///   path and the extension .csv
    /// @author SMM
    /// @date 16/01/2017
    void print_baselevel_keys(LSDFlowInfo& FlowInfo, LSDJunctionNetwork& JN, string filename);

    /// @brief This prints a basin LSDIndexRaster with basins numbered by outlet junction
    ///  and a csv file that has the latitude and longitude of both the outlet and the centroid
    /// @param FlowInfo an LSDFlowInfo object
    /// @param JN the junction network object
    /// @param Junctions The baselevel junctions to be printed
    /// @param base_filename The name of the filename to print to (should have full
    ///   path but no extension. The "_AllBasins" will be added
    /// @author SMM
    /// @date 19/01/2017
    void print_basins(LSDFlowInfo& FlowInfo, LSDJunctionNetwork& JunctionNetwork,
                               vector<int> Juntions, string base_filename);

    /// @brief This prints a csv file with all the data from the data maps
    ///  the columns are:
    ///  latitude,longitude,chi,elevation,flow distance,drainage area,m_chi,b_chi
    /// @param FlowInfo an LSDFlowInfo object
    /// @param filename The name of the filename to print to (should have full
    ///   path and the extension .csv
    /// @author SMM
    /// @date 02/06/2016
    void print_data_maps_to_file_full(LSDFlowInfo& FlowInfo, string filename);

    /// @brief This prints a csv file with a subset of the data from the data maps
    ///  the columns are:
    ///  latitude,longitude,m_chi,b_chi
    /// @param FlowInfo an LSDFlowInfo object
    /// @param filename The name of the filename to print to (should have full
    ///   path and the extension .csv
    /// @author SMM
    /// @date 02/06/2016
    void print_data_maps_to_file_full_knickpoints(LSDFlowInfo& FlowInfo, string filename);

    /// @brief This prints a csv file with a subset of the data from the data maps
    ///  the columns are:
    ///  latitude,longitude,m_chi,b_chi,knickpoint
    /// Development function
    /// @param FlowInfo an LSDFlowInfo object
    /// @param filename The name of the filename to print to (should have full
    ///   path and the extension .csv
    /// @author SMM/BG
    /// @date 02/06/2016
    void print_data_maps_to_file_basic(LSDFlowInfo& FlowInfo, string filename);

  protected:
    ///Number of rows.
    int NRows;
    ///Number of columns.
    int NCols;
    ///Minimum X coordinate.
    float XMinimum;
    ///Minimum Y coordinate.
    float YMinimum;

    ///Data resolution.
    float DataResolution;
    ///No data value.
    int NoDataValue;

    ///A map of strings for holding georeferencing information
    map<string,string> GeoReferencingStrings;

    // Some maps to store the data
    /// A map of the M_chi values. The indices are node numbers from FlowInfo
    map<int,float> M_chi_data_map;
    /// A map of the M_chi values. The indices are node numbers from FlowInfo
    map<int,float> b_chi_data_map;
    /// A map of the M_chi values. The indices are node numbers from FlowInfo
    map<int,float> elev_data_map;
    /// A map of the M_chi values. The indices are node numbers from FlowInfo
    map<int,float> chi_data_map;
    /// A map of the M_chi values. The indices are node numbers from FlowInfo
    map<int,float>  flow_distance_data_map;
    /// A map of the M_chi values. The indices are node numbers from FlowInfo
    map<int,float>  drainage_area_data_map;
    /// A map that holds elevations regressed from fitted sections.
    map<int,float> segmented_elevation_map;
    /// A map that holds segment numbers: used with skip = 0. Can be used to map
    /// distinct segments
    map<int,int> segment_counter_map;
    /// A map that holds knickpoints information
    map<int,float> segment_counter_knickpoint_map;
    /// A map that holds knickpoints signs
    map<int,int> segment_knickpoint_sign_map;
    /// A map that holds knickpoints signs
    map<int,int> segment_length_map;

    /// A vector to hold the order of the nodes. Starts from longest channel
    /// and then works through sources in descending order of channel lenght
    vector<int> node_sequence;

    /// vectors to hold the source nodes and the outlet nodes
    /// The source keys are indicies into the source_to_key_map.
    /// In big DEMs the node numbers become huge so for printing efficiency we
    /// run a key that starts at 0
    map<int,int> source_keys_map;
    /// This holds the baselevel key of each node. Again used for visualisation
    map<int,int> baselevel_keys_map;
    /// This is a map where the sources are linked to the source nodes.
    /// The key is the node index and the value is the source number
    map<int,int> key_to_source_map;
    /// This is a map where the baselevel keys are linked to the baselelvel nodes.
    map<int,int> key_to_baselevel_map;

  private:
    void create(LSDRaster& Raster);
    void create(LSDIndexRaster& Raster);
    void create(LSDFlowInfo& FlowInfo);
    void create(LSDJunctionNetwork& JN);
};

#endif
