/** @file LSDIndexChannelTree.hpp
@author Simon M. Mudd, University of Edinburgh
@author David Milodowski, University of Edinburgh
@author Martin D. Hurst, British Geological Survey
@author Stuart W. D. Grieve, University of Edinburgh
@author Fiona Clubb, University of Edinburgh

@version Version 1.0.0
@brief This object spawns vectors of LSDIndexChannels.
@details They can be indexed by the LSDCahnnel network, but can also be independent of the
channel network, storing longest channels from sources, for example
This object is designed to be flexible, it can be used either with the
LSDFlowInfo or LSDChannelNetwork object

@date 30/09/2012
*/

#include <vector>
#include "TNT/tnt.h"
#include "LSDFlowInfo.hpp"
#include "LSDRaster.hpp"
#include "LSDIndexChannel.hpp"
#include "LSDChannel.hpp"
#include "LSDFlowInfo.hpp"
#include "LSDChannelNetwork.hpp"
using namespace std;
using namespace TNT;

#ifndef LSDIndexChannelTree_H
#define LSDIndexChannelTree_H

/// @brief This object spawns vectors of LSDIndexChannels.
class LSDIndexChannelTree
{
	public:
  /// @brief Create an LSDIndexChannelTree object from a starting junction.
  /// @param FlowInfo LSDFlowInfo object. 
  /// @param ChannelNetwork LSDChannelNetwork object.	
  /// @param starting_junction Starting junction.
	LSDIndexChannelTree(LSDFlowInfo& FlowInfo, LSDChannelNetwork& ChannelNetwork, int starting_junction)
							{ create(FlowInfo, ChannelNetwork, starting_junction); }
	
  /// @brief Create an LSDIndexChannelTree object from a starting junction and orginisation switch.
  /// @param FlowInfo LSDFlowInfo object. 
  /// @param ChannelNetwork LSDChannelNetwork object.	
  /// @param starting_junction Starting junction.
  /// @param org_switch Organization switch.
  /// @param DistanceFromOutlet LSDRaster of distances from the outlet.
  LSDIndexChannelTree(LSDFlowInfo& FlowInfo, LSDChannelNetwork& ChannelNetwork,
	                    int starting_junction, int org_switch, LSDRaster& DistanceFromOutlet)
					       	{ create(FlowInfo, ChannelNetwork, starting_junction, org_switch, DistanceFromOutlet); }
	
  /// @brief Create an LSDIndexChannelTree object from a starting junction, orginisation switch and pruning parameters.
  ///
  /// @details pruning_switch == 0  channels are only added if they exceed a threshold drainage area \n
  /// pruning_switch == 1  channels are only added if the ratio between them and the mainstem
  ///						exceeds a certain value (pruning_threshold) \n
  /// pruning_switch == 2	channels are only added if the ratio between them and the area of the
  ///						mainstem _at the junction_ exceeds a certain value \n
  /// @param FlowInfo LSDFlowInfo object. 
  /// @param ChannelNetwork LSDChannelNetwork object.	
  /// @param starting_junction Starting junction.
  /// @param org_switch Organization switch.
  /// @param DistanceFromOutlet LSDRaster of distances from the outlet.
  /// @param pruning_switch Pruning switch.
  /// @param pruning_threshold Pruning threshold.
  LSDIndexChannelTree(LSDFlowInfo& FlowInfo, LSDChannelNetwork& ChannelNetwork,
	                    int starting_junction, int org_switch, LSDRaster& DistanceFromOutlet,
	                    int pruning_switch, double pruning_threshold)
					       	{ create(FlowInfo, ChannelNetwork, starting_junction, org_switch, DistanceFromOutlet,
							         pruning_switch, pruning_threshold); }

	// access function
	/// @return Vector of index channels.
	vector< LSDIndexChannel > get_LSDIndexChannelVector()	{ return IndexChannelVector; }


  /// @brief This function calcualtes the chi value starting from the bottom node of the channel tree and working its way up.
  /// @details Note that junctions are the top of the channel.
  /// @param FlowInfo LSDFlowInfo object. 
  /// @param ChannelNetwork LSDChannelNetwork object.	
  /// @param m_over_n Vector of m over n values.
  /// @param A_0 A_0 value.
  /// @return Vector of chi values.
	vector< vector<double> > calculate_chi_from_channel_tree(LSDFlowInfo& FlowInfo, LSDChannelNetwork& ChannelNetwork,
											double m_over_n, double A_0);

  /// @brief This function prints chi values. 
  /// @details It is used on the channel tree when channels are organized by links.
  /// @param Elevation LSDRaster of elevation.
  /// @param FlowInfo LSDFlowInfo object. 
  /// @param ChannelNetwork LSDChannelNetwork object.	
  /// @param m_over_n Vector of m over n values.
  /// @param A_0 A_0 value.
  /// @param chi_vs_elev_fname Output filename. 
	void print_chi_vs_elevation_from_channel_tree(LSDRaster& Elevation, LSDFlowInfo& FlowInfo, LSDChannelNetwork& ChannelNetwork,
                                                     double m_over_n, double A_0, string chi_vs_elev_fname);

	/// @brief This calculates the best fit m over n on the main stem channel.
	/// @details Minimizes the R^2 of the main stem channel assuming it is in steady state that is assuming the entire main stem is undergoing the same uplift.
  /// @param m_over_n_values Vector of m over n values.
  /// @param R_squared Vector of R-Squared values.
  /// @param A_0 A_0 value.
  /// @param FlowInfo LSDFlowInfo object. 
  /// @param Elevation_Raster LSDRaster of elevation.	
  /// @param start_movn
  /// @param increment_movn
  /// @param n_movn
  /// @return Best fit m over n.
  double fit_m_over_n_mainstem(vector<double>& m_over_n_values, vector<double>& R_squared,
				    	double A_0, LSDFlowInfo& FlowInfo, LSDRaster& Elevation_Raster,
				      	double start_movn, double increment_movn, int n_movn);

	/// @brief This function takes the channel tree and prints it to an LSDIndexRaster.
	/// @return LSDIndexRaster of the channel tree.
	LSDIndexRaster TributaryArray_to_LSDIndexRaster();

	/// @brief This creates a vector of LSDChannels, they contain area and chi information.
  /// @param m_over_n m over n ratio.	
	/// @param A_0 A_0 value.
  /// @param FlowInfo LSDFlowInfo object. 
  /// @param Elevation_Raster LSDRaster of elevation.
  /// @return Vector of LSDChannels.
  vector<LSDChannel> retrieve_LSDChannels_from_tree(double m_over_n, double A_0, LSDFlowInfo& FlowInfo,
                             LSDRaster& Elevation_Raster);

  /// @brief This function uses the segment fitting tool to look for the best fit values of m over n.
	/// @param A_0 A_0 value.
  /// @param n_movern
  /// @param d_movern
  /// @param start_movern
  /// @param minimum_segment_length
  /// @param sigma Sigma value.
  /// @param target_nodes
  /// @param FlowInfo LSDFlowInfo object. 
  /// @param Elevation_Raster LSDRaster of elevation.
  /// @param fname Output filename.
  /// @return Best fit m over n ratio. 
  double search_for_best_fit_m_over_n(double A_0, int n_movern, double d_movern,double start_movern,
                                          int minimum_segment_length, double sigma, int target_nodes,
					                      LSDFlowInfo& FlowInfo,  LSDRaster& Elevation_Raster,  string fname);

	/// @brief This prints a file that contiains all the channel information. It can be used to plot and analyze the channel profiles.
	/// @details The file format is: channel_number node_index row column flow_dist chi elevation drainage_area.
	/// @param m_over_n m over n ratio.
	/// @param A_0 A_0 value.
  /// @param FlowInfo LSDFlowInfo object. 
  /// @param Elevation_Raster LSDRaster of elevation.
  /// @param FlowDistance LSDRaster of flow length.
  /// @param fname Output filename.	
	void print_LSDChannels_from_tree(double m_over_n, double A_0, LSDFlowInfo& FlowInfo,
                             LSDRaster& Elevation_Raster, LSDRaster& FlowDistance, string fname);

	/// @brief This prints all the channels for ingestion into the chi analysis object.
	/// @details Data extracted form this file can be used in a standalone chi analysis function.
	/// The file format is: channel_number node_index row column flow_dist chi elevation drainage_area.
  /// @param FlowInfo LSDFlowInfo object. 
  /// @param Elevation_Raster LSDRaster of elevation.
  /// @param FlowDistance LSDRaster of flow length.
  /// @param fname Output filename.
	void print_LSDChannels_for_chi_network_ingestion(LSDFlowInfo& FlowInfo,
                             LSDRaster& Elevation_Raster, LSDRaster& FlowDistance, string fname);

	// get functions

	/// @return Number of rows as an integer.
	int get_NRows() const				{ return NRows; }
	/// @return Number of columns as an integer.
  int get_NCols() const				{ return NCols; }
  /// @return Minimum X coordinate as an integer.
	double get_XMinimum() const			{ return XMinimum; }
	/// @return Minimum Y coordinate as an integer.
	double get_YMinimum() const			{ return YMinimum; }
	/// @return Data resolution as an integer.                            
	double get_DataResolution() const	{ return DataResolution; }
	/// @return No Data Value as an integer.
	int get_NoDataValue() const			{ return NoDataValue; }
	/// @return Raster values as a 2D Array.

	protected:

  /// @brief Number of rows.
  int NRows;
  ///Number of columns.
	int NCols;
	///Minimum X coordinate.
  double XMinimum;
	///Minimum Y coordinate.
	double YMinimum;

	///Data resolution.
	double DataResolution;
	///No data value.
	int NoDataValue;

  ///Outlet Junction.
	int outlet_junction;
	///Outlet node.
  int outlet_node;
  
  /// @brief There are a number of ways to organize this data and this switch tells the object how its data are organized.
  ///
  /// @details It will reject member function operations if the data type is incorrect.
	int organization_switch;
  						
  ///Vector of upstream junctions. 						
	vector<int> upstream_junction_list;
	
	/// A vector containing all the index channel nodes.
	vector< LSDIndexChannel > IndexChannelVector;	

  /// Vector of reciever channels. 
	vector<int> receiver_channel;
	/// Vector of nodes along reciever channel.
	vector<int> node_on_receiver_channel;

	private:
	void create(LSDFlowInfo& FlowInfo, LSDChannelNetwork& ChannelNetwork, int starting_junction);
	void create(LSDFlowInfo& FlowInfo, LSDChannelNetwork& ChannelNetwork,
									int starting_junction, int org_switch, LSDRaster& DistanceFromOutlet);
	void create(LSDFlowInfo& FlowInfo, LSDChannelNetwork& ChannelNetwork,
									int starting_junction, int org_switch, LSDRaster& DistanceFromOutlet,
									int pruning_switch, double pruning_threshold);
};

#endif
