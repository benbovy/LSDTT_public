/** @file LSDIndexChannel.hpp
@author Simon M. Mudd, University of Edinburgh
@author David Milodowski, University of Edinburgh
@author Martin D. Hurst, British Geological Survey
@author Stuart W. D. Grieve, University of Edinburgh
@author Fiona Clubb, University of Edinburgh

@version Version 0.0.1
@brief This object contains the node indexes as well as the
row and col indices for individual channel segments.
@details These indexes could be arranged arbitrailiy according to channel
junctions or simply nodes downstream of a given node and upstream
 of another arbitrary node EndNode.

@date 30/08/2012
*/

#include <vector>
#include "TNT/tnt.h"
#include "LSDFlowInfo.hpp"
using namespace std;
using namespace TNT;

#ifndef LSDIndexChannel_H
#define LSDIndexChannel_H

///@brief This object contains the node indexes as well as the
///row and col indices for individual channel segments.
class LSDIndexChannel
{
	public:
	/// @brief The create function. This is default and throws an error.
	LSDIndexChannel()		{ create(); }
  /// @brief Create LSDIndexChannel object between a start and end node.
  /// @param StartNode Integer starting node.
  /// @param EndNode Integer ending node.
  /// @param FlowInfo LSDFlowInfo object.  
  LSDIndexChannel(int StartNode, int EndNode, LSDFlowInfo& FlowInfo)
							{ create(StartNode, EndNode, FlowInfo); }
  /// @brief Create LSDIndexChannel object between starting junction and node and ending junction and node.
  /// @param StartJunction Integer starting junction.
  /// @param StartNode Integer starting node.
  /// @param EndJunction Integer ending junction.
  /// @param EndNode Integer ending node.
  /// @param FlowInfo LSDFlowInfo object. 
  LSDIndexChannel(int StartJunction, int StartNode,
	                int EndJunction, int EndNode, LSDFlowInfo& FlowInfo)
							{ create(StartJunction,StartNode,
							  EndJunction,EndNode, FlowInfo); }

	// get functions
	
	/// @return Starting junction ID.
  int get_StartJunction() const			{ return StartJunction; }
	/// @return Ending junciton ID.
  int get_EndJunction() const				{ return EndJunction; }
	/// @return Starting node ID.
  int get_StartNode() const				{ return StartNode; }
	/// @return Ending Node ID.
  int get_EndNode() const					{ return EndNode; }
  
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

  /// @return Get vector of row indexes.
	vector<int> get_RowSequence() const		{ return RowSequence; }
	/// @return Get vector of column indexes.
  vector<int> get_ColSequence() const		{ return ColSequence; }
	/// @return Get vector of node indexes. 
  vector<int> get_NodeSequence() const	{ return NodeSequence; }

	/// @return This tells how many nodes are in the channel.
	int get_n_nodes_in_channel() const		{return int(NodeSequence.size()); }

  /// @brief This gets the node index at a given node in the index channel.
  /// @param n_node index of target node.
	/// @return Node index. 
	int get_node_in_channel(int n_node);

  /// @brief Get the number of contributing pixels at a given node in the channel.
	/// @param n_node index of target node.
  /// @param FlowInfo LSDFlowInfo object.
  /// @return Contributing pixels.
	int get_contributing_pixels_at_node(int n_node, LSDFlowInfo& FlowInfo);

	/// @brief This gets the node, row, and column index.
	/// @param n_node index of target node.
	/// @param node Blank variable for output node.
	/// @param row Blank variable for output row index.
	/// @param col Blank variable for output column index.
  /// @return Node, row, and column index.
	void get_node_row_col_in_channel(int n_node, int& node, int& row, int& col);

  /// @brief This gets the contriubting pixels at the outlet of the channel.
  /// @param FlowInfo LSDFlowInfo object.
	/// @return Contriubting pixels at the outlet of the channel.
	int get_contributing_pixels_at_outlet(LSDFlowInfo& FlowInfo)
								{ return FlowInfo.retrieve_contributing_pixels_of_node(EndNode); }
	/// @brief Gets the pixels at the penultimate node.
	///
  /// @details Useful when calculating area of basins where the tributary junction is at the EndNode.
  /// @param FlowInfo LSDFlowInfo object.
  /// @return Pixels at the penultimate node.
	int get_contributing_pixels_at_penultimate_node(LSDFlowInfo& FlowInfo);

	/// @return This prints the channel to an LSDIndexRaster in order to see where the channel is.
	LSDIndexRaster print_index_channel_to_index_raster();

	protected:

	///Number of rows.
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

  /// The starting junction (numbered within LSDChannelNetwork object).
	int StartJunction;
	/// The node index of the starting Junction (numbered in the FlowInfo object).
  int StartNode;			
	/// The ending junction (numbered within LSDChannelNetwork object).
  int EndJunction;
	/// The node index of the ending Junction (numbered in the FlowInfo object).
  int EndNode;

  ///Vector of row indices.
	vector<int> RowSequence;
	///Vector of column indices.
  vector<int> ColSequence;
	///Vector of node indices.
  vector<int> NodeSequence;

	private:
	void create();
	void create(int StartNode, int EndNode, LSDFlowInfo& FlowInfo);
	void create(int StartJunction, int StartNode,
	            int EndJunction, int EndNode, LSDFlowInfo& FlowInfo);


};



#endif
