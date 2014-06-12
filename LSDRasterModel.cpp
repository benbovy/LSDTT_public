//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// LSDRasterModel.cpp
// cpp file for the LSDRasterModel object
// LSD stands for Land Surface Dynamics
// This object provides an environment for landscape evolution modelling, which can then
// be integrated with the topographic analysis tools to efficiently analyse model runs.
//                                                                                     
// The landscape evolution model uses implicit methods to provide stability with
// relatively long timesteps.  Fluvial erosion is solved following Braun and Willet (2013)
// using the fastscape algorithm, whilst hillslope sediment transport is modelled as a
// non-linear diffusive sediment flux, following the implicit scheme developed for
// MuDDPile.
//
// The aim is to have two complimentary models:
// i) a simple coupled hillslope-channel model in which large scale landscape dynamics
// can be modelled
// ii) a more complex treatment of hillslopes explicitly incorporating the role of
// vegetation in driving sediment production and transport, and that copes with the  
// with the transition from soil mantled-bedrock hillslopes at high erosion rates.
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// This object is written by
// Simon M. Mudd, University of Edinburgh
// David T. Milodowski, University of Edinburgh
// Martin D. Hurst, British Geological Survey
// Fiona Clubb, University of Edinburgh
// Stuart Grieve, University of Edinburgh
// James Jenkinson, University of Edinburgh
//
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// Version 0.0.1		24/07/2013
//
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <string>
#include <math.h>
#include <string.h>
#include <ctime>
#include <cstdlib>
#include "TNT/tnt.h"
#include "TNT/jama_lu.h"
#include "TNT/jama_eig.h"
#include <boost/numeric/mtl/mtl.hpp>
#include <boost/numeric/itl/itl.hpp>
#include <python2.7/Python.h>
#include "LSDRaster.hpp"
#include "LSDFlowInfo.hpp"
//#include "LSDFlowInfo_alt.hpp"
#include "LSDRasterSpectral.hpp"
#include "LSDStatsTools.hpp"
#include "LSDIndexRaster.hpp"
#include "LSDRasterModel.hpp"
using namespace std;
using namespace TNT;
using namespace JAMA;

#define PI 3.14159265358
#ifndef LSDRasterModel_CPP
#define LSDRasterModel_CPP


LSDRasterModel& LSDRasterModel::operator=(const LSDRasterModel& rhs)
 {
	if (&rhs != this)
	 {
	  create(rhs.get_NRows(),rhs.get_NCols(),rhs.get_XMinimum(),rhs.get_YMinimum(),
	         rhs.get_DataResolution(),rhs.get_NoDataValue(),rhs.get_RasterData());
	 }
	return *this;
 }

// the create function. 
// This sets up a model domain with a default size and model parameters
void LSDRasterModel::create()
{
	NRows = 100;
	NCols = 100;
	DataResolution = 10;
	NoDataValue = -99;
	XMinimum = 0;
	YMinimum = 0;
	RasterData = Array2D <float> (NRows, NCols, 0.0);

	default_parameters();
}

// this creates a raster using an infile
void LSDRasterModel::create(string filename, string extension)
{
	read_raster(filename,extension);
}

// this creates a raster filled with no data values
void LSDRasterModel::create(int nrows, int ncols, float xmin, float ymin,
	          float cellsize, float ndv, Array2D<float> data)
{
	NRows = nrows;
	NCols = ncols;
	XMinimum = xmin;
	YMinimum = ymin;
	DataResolution = cellsize;
	NoDataValue = ndv;

	RasterData = data.copy();

	if (RasterData.dim1() != NRows)
	{
		cout << "dimension of data is not the same as stated in NRows!" << endl;
		exit(EXIT_FAILURE);
	}
	if (RasterData.dim2() != NCols)
	{
		cout << "dimension of data is not the same as stated in NCols!" << endl;
		exit(EXIT_FAILURE);
	}

}

// this creates a LSDRasterModel raster from another LSDRaster
void LSDRasterModel::create(LSDRaster& An_LSDRaster)
{
	NRows = An_LSDRaster.get_NRows();
	NCols = An_LSDRaster.get_NCols();
	XMinimum = An_LSDRaster.get_XMinimum();
	YMinimum = An_LSDRaster.get_YMinimum();
	DataResolution = An_LSDRaster.get_DataResolution();
	NoDataValue = An_LSDRaster.get_NoDataValue();

	RasterData = An_LSDRaster.get_RasterData();
}


LSDRasterModel::LSDRasterModel(int NRows, int NCols)
{
	this->NRows = NRows;
	this->NCols = NCols;
	this->DataResolution = 10;
	this->NoDataValue = -99;
	XMinimum = 0;
	YMinimum = 0;
	RasterData = Array2D <float> (NRows, NCols, 0.0);
}

// this creates an LSDRasterModel using a master parameter file
void LSDRasterModel::create(string master_param)
{
	NRows = 100;
	NCols = 100;
	DataResolution = 10;
	NoDataValue = -99;
	XMinimum = 0;
	YMinimum = 0;

	default_parameters();
	initialize_model(master_param);
}

// this sets default parameters for the model
void LSDRasterModel::default_parameters( void )
{
	initialized = false;
	name = "LSDRM";
	report_name = "LSDRM";
	reporting = true;
	vector <string> bc(4, "n");					// Initialise boundaries to No flow
	bc[0] = "b";
	bc[1] = "p";
	bc[2] = "b";
	bc[3] = "p";
	set_boundary_conditions( bc );					// Set these as default boundary conditions

	set_uplift( 0, 0.0005 );					// Block uplift, 0.005mm.yr^{-1}

	set_timeStep( 100 );						// 100 years
	set_endTime( 10000 );
	endTime_mode = 0;
	set_num_runs( 1 );
	set_K( 0.0002 );
	set_D( 0.02 );
	set_rigidity( 1E7 );
	set_m( 0.5 );
	set_n( 1 );
	set_threshold_drainage( -99 );					// Not used if negative
	set_S_c( 30 );							// 30 degrees 
	set_print_interval( 10 );						// number of timesteps
	set_steady_state_tolerance( 0.00001 );
	current_time = 0;
	noise = 0.1;

	K_mode = 0;
	D_mode = 0;
	periodicity   = 10000;
	periodicity_2 = 20000;
	period_mode = 1;
	switch_time = endTime/2;
	p_weight = 0.8;
	K_amplitude = 0.001;
	D_amplitude = 0.001;
	report_delay = 0;

	print_elevation = true;
	print_hillshade = false;
	print_erosion = false;
	print_erosion_cycle = false;
	print_slope_area = false;

	quiet =		false;
	fluvial = 	true;
	hillslope = 	true;
	nonlinear =	false;
	isostasy = 	false;
	flexure =	false;

	steady_state_tolerance = 0.0001;
	steady_state_limit = -1;

	initialized = false;
	cycle_steady_check = false;
}
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// INITIALISATION MODULE
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// This module initialises the model runs, calling the required function from
// the initial topography and loads the parameters from the parameter file.
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//  Initialise_model
//----------------------------------------------------------------------------
void LSDRasterModel::initialize_model(
	string& parameter_file, string& run_name, float& dt, float& EndTime, float& PrintInterval,
	float& k_w, float& b, float& m, float& n, float& K, float& ErosionThreshold, 
	float& K_nl, float& S_c, float& UpliftRate, float& PrecipitationRate,
	float& NorthBoundaryElevation, float& SouthBoundaryElevation,
	Array2D<float>& PrecipitationFlux, Array2D<float>& SlopesBetweenRows,
	Array2D<float>& SlopesBetweenColumns, Array2D<float>& ErosionRate)
{
		// load the parameters
	// each parameter in the param file is preceded by its name
	// these MUST be in the correct order
	// the names MUST NOT have spaces
	string temp;
	ifstream param_in;
	param_in.open(parameter_file.c_str());

	param_in >> temp >> run_name;
	cout << "run name is: " << run_name;
	param_in >> temp >> dt;
	param_in >> temp >> EndTime;
	param_in >> temp >> PrintInterval;
	cout << "dt: " << dt << " end_time: " << EndTime << " print_interval: " << PrintInterval << endl;
	param_in >> temp >> k_w;
	param_in >> temp >> b;
	param_in >> temp >> m;
	param_in >> temp >> n;
	param_in >> temp >> K;
	param_in >> temp >> ErosionThreshold;
	cout << "k_w: " << k_w << " b: " << b << " m: " << m << " n: " << n << " K: " << K << " eros_thresh: " << ErosionThreshold << endl;
	param_in >> temp >> K_nl;
	param_in >> temp >> S_c;
	cout << "D_nl: " << K_nl << " S_c: " << S_c << endl;
	param_in >> temp >> UpliftRate;
	param_in >> temp >> PrecipitationRate;
	cout << "uplift_rate: " << UpliftRate << " precip_rate: " << PrecipitationRate << endl;
	param_in >> temp >> NorthBoundaryElevation;
	param_in >> temp >> SouthBoundaryElevation;
	cout << "N bdry elev: " << NorthBoundaryElevation << " S bdry elev: " << SouthBoundaryElevation << endl;

// 	string surface_file;
// 	param_in >> surface_file;
// 	
// 	string file_extension;
// 	param_in >> file_extension;
// 	
// 	cout << "Surface_file is: " << surface_file << endl;
	
	param_in.close();
	
	float dx = get_DataResolution();
	float dy = get_DataResolution();
	cout << " NRows: " << NRows << " NCols: " << NCols << " dx: " << dx << " dy: " << dy
	     << " xllcorn: " << XMinimum << " yllcorn: " << YMinimum << endl;

	// now set up some arrays
	// first the precipitation array
	PrecipitationFlux = precip_array_from_precip_rate(PrecipitationRate);

	// set up slope arrays of the correct size
	Array2D<float> slopes_between_rows_temp(NRows+1,NCols,0.0);
	Array2D<float> slopes_between_columns_temp(NRows,NCols+1,0.0);
	SlopesBetweenRows = slopes_between_rows_temp.copy();
	SlopesBetweenColumns = slopes_between_columns_temp.copy();

	// set up erosion rate array of the correct size
	Array2D<float> temp_erate(NRows,NCols,0.0);
	ErosionRate = temp_erate.copy();
}

// -----------------------------------------------------------------------
// Alternative mode of initialising from parameter file
//
// Loads parameters using the void parse_line method found in LSDStatsTools
// Parameters are loaded into intrinsic class attributes
// Part of a move to using attributes vs passing through functions
//  ----------------------------------------------------------------------


void LSDRasterModel::initialize_model(string param_file)
{
	bool loaded_from_file = false;
	initialized = true;
	ifstream infile;
	infile.open(param_file.c_str());
	string parameter, value, lower;

	while (infile.good())
	{
		parse_line(infile, parameter, value);
		lower = parameter;
		if (parameter == "NULL")
			continue;
		for (unsigned int i=0; i<parameter.length(); ++i)
			lower[i] = tolower(parameter[i]);

		if 	(lower == "run name")		name 		= value;
		else if (lower == "time step")		timeStep 	= atof(value.c_str());
		else if (lower == "end time")		endTime 	= atof(value.c_str());
		else if (lower == "num runs")		num_runs	= atoi(value.c_str());
		else if (lower == "end time mode")	endTime_mode	= atoi(value.c_str());
		else if (lower == "max uplift")		max_uplift 	= atof(value.c_str());
		else if (lower == "uplift mode")	uplift_mode 	= atoi(value.c_str());
		else if (lower == "tolerance")		steady_state_tolerance = atof(value.c_str());
		else if (lower == "steady limit")	steady_state_limit = atof(value.c_str());
		else if (lower == "boundary code")	for (int i=0; i<4; ++i) boundary_conditions[i] = value[i];
		else if (lower == "m")			m 		= atof(value.c_str());
		else if (lower == "n")			n 		= atof(value.c_str());
		else if (lower == "k")			K_fluv 		= atof(value.c_str());
		else if (lower == "threshold drainage") threshold_drainage = atof(value.c_str());	
		else if (lower == "d")			K_soil 		= atof(value.c_str());
		else if (lower == "s_c")		S_c 		= atof(value.c_str());
		else if (lower == "rigidity")		rigidity	= atof(value.c_str());
		else if (lower == "nrows"){		if (not loaded_from_file) 	NRows 		= atoi(value.c_str());}
		else if (lower == "ncols"){		if (not loaded_from_file) 	NCols 		= atoi(value.c_str());}
		else if (lower == "resolution"){	if (not loaded_from_file) 	DataResolution 	= atof(value.c_str()); }
		else if (lower == "print interval")	print_interval	= atoi(value.c_str());
		else if (lower == "k mode")		K_mode		= atoi(value.c_str());
		else if (lower == "d mode")		D_mode 		= atoi(value.c_str());
		else if (lower == "periodicity")	periodicity 	= atof(value.c_str());
		else if (lower == "periodicity 2")	periodicity_2   = atof(value.c_str());
		else if (lower == "P ratio")		{p_weight	= atof(value.c_str()); if (p_weight > 1) p_weight = 1;}
		else if (lower == "period mode")	period_mode	= atoi(value.c_str());
		else if (lower == "switch time")	switch_time	= atof(value.c_str());
		else if (lower == "k amplitude")	K_amplitude	= atof(value.c_str()) * K_fluv;
		else if (lower == "d amplitude")	D_amplitude 	= atof(value.c_str()) * K_soil;
		else if (lower == "noise")		noise		= atof(value.c_str());
		else if (lower == "report delay")	report_delay 	= atof(value.c_str());

		else if (lower == "fluvial")		fluvial 	= (value == "on") ? true : false;
		else if (lower == "hillslope")		hillslope 	= (value == "on") ? true : false;
		else if (lower == "non-linear")		nonlinear 	= (value == "on") ? true : false;
		else if (lower == "isostasy")		isostasy 	= (value == "on") ? true : false;
		else if (lower == "flexure")		flexure 	= (value == "on") ? true : false;
		else if (lower == "quiet")		quiet		= (value == "on") ? true : false;
		else if (lower == "reporting")		reporting	= (value == "on") ? true : false;
		else if (lower == "print elevation")	print_elevation = (value == "on") ? true : false;
		else if (lower == "print hillshade")	print_hillshade = (value == "on") ? true : false;
		else if (lower == "print erosion")	print_erosion   = (value == "on") ? true : false;
		else if (lower == "print erosion cycle") print_erosion_cycle = (value == "on") ? true : false;
		else if (lower == "print slope-area")	print_slope_area= (value == "on") ? true : false;

		else if	(lower == "load file")
		{
			ifstream file(value.c_str());
			if (file)
			{
				file.close();
				read_raster(value.substr(0, value.find(".")), value.substr(value.find(".")+1));
				loaded_from_file = true;
			}
			else
				cerr << "Warning, file '" << value << "' not found" << endl;
		}

		else	cout << "Line " << __LINE__ << ": No parameter '" << parameter << "' expected.\n\t> Check spelling." << endl;
	}
	//if (hillslope)
	//	steady_state_tolerance *= pow(10, 2.8);
	if (name != "")
		report_name = name;
	else
		report_name = param_file;
	if (not loaded_from_file)
	{
		RasterData = Array2D<float>(NRows, NCols, 0.0);
		// Generate random noise
		random_surface_noise(0, noise);
		// Fill the topography
		LSDRaster *temp;
		temp = new LSDRaster(*this);
		float thresh_slope = 0.00001;
		*temp = fill(thresh_slope);
		RasterData = temp->get_RasterData();
		delete temp;
	}
	root_depth = Array2D<float>(NRows, NCols, 0.0);
	current_time = 0;
}

void LSDRasterModel::check_steady_state( void )
{
	steady_state = true;
	if (cycle_steady_check)
	{
		for (int i=0; i<4; ++i)
		{
			if (erosion_cycle_record[i] == -99 || abs(erosion_cycle_record[i] - erosion_cycle_record[i+1]) > steady_state_tolerance)
			{
				steady_state = false;
				return;
			}
		}
	}
	else if (steady_state_limit < 0 || current_time < steady_state_limit)
	{
		for (int i=0; i<NRows; ++i)
		{
			for (int j=0; j<NCols; ++j)
			{
				if (abs(RasterData[i][j] - zeta_old[i][j]) > steady_state_tolerance)
				{
					steady_state = false;
					return;
				}
			}
		}
	}
	if (not initial_steady_state)
	{
		initial_steady_state = true;
		time_delay = current_time;
		if (endTime_mode == 1 || endTime_mode == 3)
			endTime += time_delay;
		if (not quiet)
		{
			cout << "\t\t\t> Initial steady state reached at " << current_time;
		}
	}
}

void LSDRasterModel::check_recording( void )
{
	int num_cycles = (current_time - time_delay) / periodicity;
	if (recording)
	{
		return;
	}
	else if (not initial_steady_state)
	{
		// If we haven't reached steady state yet, don't record any data
		recording = false;
	}
	else if (K_mode == 0 && D_mode == 0)
	{
		// If we aren't changing any of the forcing parameters, we can record
		// as soon as we hit steady state
		recording = true;
	}
	else if (num_cycles >= 1)
	{
		// If we are changing forcing parameters, we need to wait until one cycle has
		// passed, as there is a small adjustment period associated with the first cycle
		recording = true;
	}
	else
	{
		recording = false;
	}
}

bool LSDRasterModel::check_end_condition( void )
{
	int num_cycles;
	if (K_mode != 0 || D_mode != 0)
		num_cycles = cycle_number-1;
	else
		num_cycles = (current_time - time_delay) / periodicity;
	float endTime_adjusted;
	switch (endTime_mode){
		case 1:		// time specified is after reaching steady state
			if (not initial_steady_state || current_time <= endTime+timeStep)
				return false;
			else
				return true;
			break;
		case 2:		// Number specified is a number of cycles of periodicity
			if (not initial_steady_state || num_cycles <= endTime)
				return false;
			else
				return true;
			break;	
		case 3:		// Time specified is after reaching steady state, but waits till a roughly exact number of cycles of periodicty have passed
			if (ceil((endTime-time_delay)/periodicity) == 1)
				endTime_adjusted = (1+ceil((endTime-time_delay) / periodicity)) * periodicity + time_delay;
			else
				endTime_adjusted = (ceil((endTime-time_delay) / periodicity)) * periodicity + time_delay;
		//	if (not quiet)
		//		cout << "\n" << endTime << " " << time_delay << " " << periodicity << " " <<endTime_adjusted << " hi " << endl;
			endTime = endTime_adjusted;
			if (not initial_steady_state || current_time < endTime_adjusted+timeStep)
				return false;
			else
				return true;
			break;
		default:
			if (current_time >= endTime)
				return true;
			else
				return false;
			break;
	};
}

void LSDRasterModel::check_periodicity_switch( void )
{
	if ((K_mode == 0 && D_mode == 0) || (not initial_steady_state && not cycle_steady_check))
		return;
	else if (period_mode == 2 || period_mode == 4)
	{
		float p = periodicity;

		float swap;
		float t;
		if (endTime_mode == 2)
			t = switch_time * p;
		else if (endTime_mode == 3)
			t = ceil((switch_time)/p) * p;
		else
			t = switch_time;

		if (current_time-time_delay > t + switch_delay)
		{
			// Time to switch periodicities yo
			/// Possible problem here, if running sequential models 
			/// we can't remeber which periodicity was the original one
			swap = periodicity;
			periodicity = periodicity_2;
			periodicity_2 = swap;
			// Bump up the time til the next switch
			switch_delay = current_time - time_delay - timeStep;
		}
	}
}

bool LSDRasterModel::check_if_hung( void )
{
	int num_cycles = (current_time) / periodicity;
	return false;
	switch (endTime_mode){
		case 1:
			if (not initial_steady_state && current_time > endTime*100)
				return true;
			else
				return false;
			break;
		case 2:
			if (not initial_steady_state && num_cycles > endTime * 100)
				return true;
			else
				return false;
			break;
		case 3:
			if (not initial_steady_state && current_time > endTime*100)
				return true;
			else
				return false;
			break;
		default:
			return false;
			break;
	};
}

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// BUFFER SURFACE
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// 2 functions to buffer the raster surface.
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// This first function is used as a simple way to implement boundary conditions,
// particularly no flux and periodic boundary conditions.
// The buffered surface has NRows+2 rows and NCols+2 columns.
// The integer b_type sets the type of boundary conditions, but currently there
// is only one implementation: no flux across N and S; periodic for E and W. 
//----------------------------------------------------------------------------
LSDRasterModel LSDRasterModel::create_buffered_surf(int b_type)
{
	Array2D<float> surf = RasterData.copy();  
 	Array2D<float> buff(NRows+2,NCols+2);
	Array2D<float> buff_surf = buff.copy();
// 
// 	switch(b_type)
// 	{
// 		case 1:
			// first set up the corners: these data points have no impact on calculations
			// but are here if there is some unforseen instability
			buff_surf[0][0] = surf[0][0];
			buff_surf[NRows+1][0] = surf[NRows-1][0];
			buff_surf[0][NCols+1] = surf[0][NCols-1];
			buff_surf[NRows+1][NCols+1] = surf[NRows-1][NCols-1];
			// now get the periodic boundaries
			for (int row = 0; row<NRows; row++)
			{
				buff_surf[row+1][0] = surf[row][NCols-1];
				buff_surf[row+1][NCols+1] = surf[row][0];
			}
			// now get the no flux boundaries
			for (int col = 0; col<NCols; col++)
			{
				buff_surf[0][col+1] = surf[0][col];
				buff_surf[NRows+1][col+1] = surf[NRows-1][col];
			}
			// now copy the interior
			for (int row=0; row<NRows; row++)
			{
				for (int col =0; col<NCols; col++)
				{
					buff_surf[row+1][col+1]=surf[row][col];
				}
			}			
			LSDRasterModel BufferedSurface(NRows+2, NCols+2, XMinimum-DataResolution, YMinimum-DataResolution, DataResolution, NoDataValue, buff_surf);
	    return BufferedSurface;
// 			break;
// 
// 		default:
// 			cout << "You chose and invalid boundary condition" << endl;
// 			exit(1);
// 	}
}

//------------------------------------------------------------------------------
// This second version has periodic boundaries at E and W boundaries, and 
// Neumann boundary conditions (prescribed elevations) at the N and S
// boundaries.
//------------------------------------------------------------------------------
LSDRasterModel LSDRasterModel::create_buffered_surf(float South_boundary_elevation,float North_boundary_elevation)
{
	Array2D<float> surf = RasterData.copy();  
	Array2D<float> buff(NRows+2,NCols+2);
	Array2D<float> buff_surf = buff.copy();

	// now get the periodic boundaries
	for (int row = 0; row<NRows; row++)
	{
		buff_surf[row+1][0] = surf[row][NCols-1];
		buff_surf[row+1][NCols+1] = surf[row][0];
	}
	// now get the fixed elevation boundaries
	for (int col = 0; col<NCols+2; col++)
	{
		buff_surf[0][col] = South_boundary_elevation;
		buff_surf[NRows+1][col] = North_boundary_elevation;
	}
	// now copy the interior
	for (int row=0; row<NRows; row++)
	{
		for (int col =0; col<NCols; col++)
		{
			buff_surf[row+1][col+1]=surf[row][col];
		}
	}
	LSDRasterModel BufferedSurface(NRows+2, NCols+2, (XMinimum-DataResolution), (YMinimum-DataResolution), DataResolution, NoDataValue, buff_surf);
	return BufferedSurface;
}
 
////------------------------------------------------------------------------------
//// impose_channels: this imposes channels onto the landscape
//// the row and column of the channels are stored in the c_rows and c_cols vectors
//// the elevation of the channles are stored in the c_zeta file
////------------------------------------------------------------------------------
//LSDRasterModel LSDRasterModel::impose_channels(vector<int> c_rows, vector<int> c_cols, vector<float> c_zeta)
//{
//	Array2D<float> zeta = RasterData.copy();
//  int n_channel_nodes = c_rows.size();
//
//	for (int i = 0; i<n_channel_nodes; i++)
//	{
//		zeta[ c_rows[i] ][ c_cols[i] ] = c_zeta[i];
//	}
//  LSDRasterModel Zeta(NRows, NCols, XMinimum, YMinimum, DataResolution, NoDataValue, zeta);
//  return Zeta;
//}

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// CALCULATE EROSION RATES
// Simple function that creates an array with the erosion rates for a given 
// timestep, calculated by diffencing elevation rasters with consecutive 
// timesteps.
//------------------------------------------------------------------------------
Array2D<float> LSDRasterModel::calculate_erosion_rates( void )
{
	// Array2D<float> Zeta = RasterData.copy();	// This is just a waste of memory
	Array2D<float> ErosionRateArray(NRows,NCols,NoDataValue);
	for(int row=0; row<NRows; ++row)
	{
	  for(int col=0; col<NCols; ++col)
	  {
	    if(RasterData[row][col]!=NoDataValue)
	    {
		    ErosionRateArray[row][col] = get_erosion_at_cell(row, col);
	    }
	  }
	}
	return ErosionRateArray;
}

float LSDRasterModel::get_erosion_at_cell(int i, int j)
{
      return (zeta_old[i][j]-RasterData[i][j]+get_uplift_at_cell(i,j))/timeStep;
}

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// UPLIFT SURFACE
// Apply uplift field to the raster.  Overloaded function so that the first
// simply considers uniform uplift, the second allows user to use a prescribed
// uplift fields of greater complexity
//------------------------------------------------------------------------------
// Uniform uplift
//------------------------------------------------------------------------------
LSDRasterModel LSDRasterModel::uplift_surface(float UpliftRate, float dt)
{
	Array2D<float> ZetaRaster;
	ZetaRaster = RasterData.copy();
	for(int row=0; row<NRows; ++row)
	{
		for(int col=0; col<NCols; ++col)
		{
			if(get_data_element(row,col)!=NoDataValue) 
			{
				ZetaRaster[row][col] += UpliftRate*dt;
			}
		}
	}
	LSDRasterModel Zeta(NRows, NCols, XMinimum, YMinimum, DataResolution, NoDataValue, ZetaRaster);
	return Zeta;
}

void LSDRasterModel::uplift_surface( void )
{	
	for(int row=0; row<NRows; ++row)
	{
		for(int col=0; col<NCols; ++col)
		{
			if (is_base_level(row, col))
				continue;
			if(get_data_element(row,col)!=NoDataValue) 
			{
				RasterData[row][col] += get_uplift_at_cell(row, col);
			}
		}
	}
}
//------------------------------------------------------------------------------
// Specified uplift field 
// uplift field should be specified as an array with the same dimensions as the
// elevation raster, permitting non-uniform uplift fields to be applied in the 
// model.
//------------------------------------------------------------------------------
LSDRasterModel LSDRasterModel::uplift_surface(Array2D<float> UpliftRate, float dt)
{
	Array2D<float> ZetaRaster;
	ZetaRaster = RasterData.copy();
	for(int row=0; row<NRows; ++row)
	{
		for(int col=0; col<NCols; ++col)
		{
			if(get_data_element(row,col)!=NoDataValue) 
			{
				ZetaRaster[row][col] += UpliftRate[row][col]*dt;
			}
		}
	}
	LSDRasterModel Zeta(NRows, NCols, XMinimum, YMinimum, DataResolution, NoDataValue, ZetaRaster);
	return Zeta;
}

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// CREATE PRECIPITION FLUX ARRAY
// Produces precipitation array from provided precipitation rate.
//------------------------------------------------------------------------------
Array2D<float> LSDRasterModel::precip_array_from_precip_rate(float precip_rate)
{
	float precip_flux = DataResolution*DataResolution*precip_rate;
	Array2D<float> precip_start(NRows,NCols,precip_flux);
	Array2D<float> PrecipitationFluxArray = precip_start.copy();
	return PrecipitationFluxArray;
}

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// TOPOGRAPHIC DERIVATIVES
//----------------------------------------------------------------------------
// get_slopes
//----------------------------------------------------------------------------
// Specifically, this function gets the topographic slopes, as required for
// the sediment flux calculations.  The slopes are stored as two matrices, one
// that stores slopes between rows, the other which for slopes between 
// columns.  Note that this is a finite volume model that utilises cubic model
// voxels. Sediment fluxes are only permitted through the faces.
//
// For slopes between columns, the entry at S[row][col] refers to the slope
// between zeta at node [row][col] and at node [row][col+1].  Likewise for the
// slopes between rows.  In short, the center points of the slopes are offset
// by 1/2 a node spacing in the positive direction.
// Note that there are NCols +1 and NRows +1 columns and rows respectively
//----------------------------------------------------------------------------
void LSDRasterModel::get_slopes(Array2D<float>& SlopesBetweenRows, Array2D<float>& SlopesBetweenCols)
{  
	Array2D<float> buff_zeta = RasterData.copy();
	float inv_dx = 1/DataResolution;
	float inv_dy = 1/DataResolution;
	//cout << "LINE 50 flux_funcs, n_rows: " << n_rows
	//     << " and n_cols: " << n_cols << endl;
	//cout << "LINE 52 flux_funcs, zeta: " << zeta << endl;

	for (int row = 0; row<NRows; row++)
	{
		for (int col = 0; col<=NCols; col++)
		{
			//cout << "row: " << row << " and col: " << col << endl;
			SlopesBetweenCols[row][col] = (buff_zeta[row+1][col+1] - buff_zeta[row+1][col])*inv_dx;
		}
	}

	for (int row = 0; row<=NRows; row++)
	{
		for (int col = 0; col<NCols; col++)
		{
			SlopesBetweenRows[row][col] = (buff_zeta[row+1][col+1] - buff_zeta[row][col+1])*inv_dy;
		}
	}
}

//----------------------------------------------------------------------------
// get_topographic_divergence
// gets the topographic divergence at each point in the model domain.  Use
// buffered topography
//----------------------------------------------------------------------------
Array2D<float> LSDRasterModel::get_topographic_divergence()
{
	Array2D<float> buffered_topo = RasterData.copy(); 
	Array2D<float> empty_div(NRows,NCols,0.0);
	Array2D<float> div_zeta = empty_div.copy();
	float s1,s2;
	for (int row = 0; row<NRows; row++)
	{
		for (int col = 0; col<NCols; col++)
		{
			s1 = (buffered_topo[row+1][col+2]-buffered_topo[row+1][col])*0.5/DataResolution;
			s2 = (buffered_topo[row+2][col+1]-buffered_topo[row][col+1])*0.5/DataResolution;
			div_zeta[row][col] = sqrt(s1*s1+s2*s2);
		}
	}	
	Array2D<float> TopoDivergence = div_zeta.copy();
	return TopoDivergence;
}

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// HYDROLOGICAL TOOLS
//----------------------------------------------------------------------------
// calculate_channel_width_wolman
// This function calculates channel width using the wolman method.
// NOTE: typically Q_w will be in m^3/s.
// EXAMPLE: in Salmon River, Idaho (Emmett, 1975 cited in Knighton 1988):
//          k_w = 2.77 and b = 0.56. b is often assumed to be 0.5
//----------------------------------------------------------------------------
float LSDRasterModel::calculate_channel_width_wolman(float Q_w, float k_w, float b)
{
	float ChannelWidth;
	if (b == 1.0)
	{
		ChannelWidth = Q_w*k_w;
	}
	else if (b == 0.5)
	{
		ChannelWidth = k_w*sqrt(Q_w);
	}
	else
	{
		ChannelWidth = k_w*pow(Q_w,b);
	}
	return ChannelWidth;
}
//----------------------------------------------------------------------------
// array_channel_width_wolman
// this function calcualtes channel width in a stand alone module so the widths
// can be tested
//----------------------------------------------------------------------------
Array2D<float> LSDRasterModel::array_channel_width_wolman(Array2D<float>& Q_w, float& k_w, float& b)
{
	// reset the channel width array
	Array2D<float> empty_w(NRows,NCols,1.0);
	Array2D<float> ChannelWidth = empty_w.copy();

	for (int row = 0; row<NRows; row++)
	{
		for (int col = 0; col<NCols; col++)
		{
			ChannelWidth[row][col] = calculate_channel_width_wolman(Q_w[row][col], k_w, b);
		}
	}
	return ChannelWidth;
}

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// EROSION RATES/SEDIMENT FLUXES
//
//----------------------------------------------------------------------------
// this caluclates the fluvial erosion rate at each point
//----------------------------------------------------------------------------
Array2D<float> LSDRasterModel::calculate_fluvial_erosion_rate(Array2D<float> ChannelWidth, Array2D<float> Q_w,
							Array2D<float> TopoDivergence, float K, float n, float m, float eros_thresh)							   
{
	// set up an array to populate with erosion
	Array2D<float> empty_fluv_eros(NRows,NCols,0.0);
	Array2D<float> FluvialErosionRate = empty_fluv_eros.copy();

	for (int row = 0; row<NRows; row++)
	{
		for (int col = 0; col<NCols; col++)
		{
			FluvialErosionRate[row][col] = K*(ChannelWidth[row][col]/DataResolution)*pow(TopoDivergence[row][col],n)*pow(Q_w[row][col],m) - eros_thresh;
			if (FluvialErosionRate[row][col] < 0)
			{
				FluvialErosionRate[row][col] = 0;
			}
		}
	}
	return FluvialErosionRate;
}
//------------------------------------------------------------------------------

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// IMPLICIT MODEL COMPONENTS
//------------------------------------------------------------------------------
// Implicit schemes for combination of hillslope sediment transport using
// non-linear hillslope transport law, and fluvial erosion.  This is essentially
// the implicit implementation of MuddPILE, but has been modified so that now
// fluvial erosion is undertaken using FASTSCAPE (Braun and Willet, 2013), which
// greatly increases computational efficiency.
//------------------------------------------------------------------------------
// calculate_k_values_for_assembly_matrix/mtl_initiate_assembler_matrix
// this function creates vectors of integers that refer to the k values, that is
// the index into the vectorized matrix of zeta values, that is used in the assembly matrix
// the number of elements in the k vectors is N_rows*N_cols
//------------------------------------------------------------------------------
void LSDRasterModel::calculate_k_values_for_assembly_matrix(int NRows, int NCols, vector<int>& k_value_i_j,
	                    vector<int>& k_value_ip1_j,	vector<int>& k_value_im1_j, vector<int>& k_value_i_jp1, 
	                    vector<int>& k_value_i_jm1)											
{
	int N_elements_in_k_vec = NRows*NCols;

	// initialize the vectors with empty values
	vector<int> empty_vec(N_elements_in_k_vec,0);
	k_value_i_j   = empty_vec;
	k_value_ip1_j = empty_vec;
	k_value_im1_j = empty_vec;
	k_value_i_jp1 = empty_vec;
	k_value_i_jm1 = empty_vec;

	// we loop through each node
	int counter = 0;
	for (int row = 0; row<NRows; row++)
	{
		for (int col = 0; col<NCols; col++)
		{
			k_value_ip1_j[counter] = NCols*(row+2)+col;
			k_value_im1_j[counter] = NCols*row+col;
			k_value_i_j[counter] = NCols*(row+1)+col;

			// logic for west periodic boundary
			if(col == 0)
			{
				k_value_i_jp1[counter] = NCols*(row+1)+col+1;
				k_value_i_jm1[counter] = NCols*(row+1)+NCols-1;
			}
			// logic for east periodic boundary
			else if(col == NCols-1)
			{
				k_value_i_jp1[counter] = NCols*(row+1);
				k_value_i_jm1[counter] = NCols*(row+1)+col-1;

			}
			// logic for rest of matrix
			else
			{
				k_value_i_jp1[counter] = NCols*(row+1)+col+1;
				k_value_i_jm1[counter] = NCols*(row+1)+col-1;
			}

			// increment counter
			counter++;
		}
	}
}

void LSDRasterModel::mtl_initiate_assembler_matrix(int& problem_dimension,				     
				     float& inv_dx_S_c_squared, float& inv_dy_S_c_squared, float& dx_front_term, 
	             float& dy_front_term, vector<int>& vec_k_value_i_j, vector<int>& vec_k_value_ip1_j,
	     vector<int>& vec_k_value_im1_j, vector<int>& vec_k_value_i_jp1, vector<int>& vec_k_value_i_jm1)
{
	float dx = DataResolution;
	float dy = DataResolution;
	float D = get_D();

	inv_dx_S_c_squared = 1/(dx*dx*S_c*S_c);
	inv_dy_S_c_squared = 1/(dy*dy*S_c*S_c);
	dx_front_term = timeStep*D/(dx*dx);
	dy_front_term = timeStep*D/(dy*dy);

	problem_dimension = (NRows+2)*NCols;
	calculate_k_values_for_assembly_matrix(NRows, NCols, vec_k_value_i_j, vec_k_value_ip1_j,
											vec_k_value_im1_j, vec_k_value_i_jp1, vec_k_value_i_jm1);

}

//------------------------------------------------------------------------------
// mtl_assemble_matrix
// this function assembles the solution matrix for nonlinear creep transport
//------------------------------------------------------------------------------
void LSDRasterModel::mtl_assemble_matrix(Array2D<float>& zeta_last_iter, Array2D<float>& zeta_last_timestep,
						 Array2D<float>& zeta_this_iter, Array2D<float>& uplift_rate, Array2D<float>& fluvial_erosion_rate,
	           mtl::compressed2D<float>& mtl_Assembly_matrix, mtl::dense_vector<float>& mtl_b_vector,
						 float dt, int problem_dimension, float inv_dx_S_c_squared, float inv_dy_S_c_squared, 
					   float dx_front_term, float dy_front_term,
	           float South_boundary_elevation, float North_boundary_elevation,
	           vector<int>& vec_k_value_i_j, vector<int>& vec_k_value_ip1_j,vector<int>& vec_k_value_im1_j,
						 vector<int>& vec_k_value_i_jp1, vector<int>& vec_k_value_i_jm1)
{
	// the coefficients in the assembly matrix
	float A,B,C,D;

	// reset the assembly and b vector
	mtl_Assembly_matrix = 0.0;
	mtl_b_vector = 0.0;

	// create the inserter. This is deleted when this function is exited
	mtl::matrix::inserter< mtl::compressed2D<float> > ins(mtl_Assembly_matrix);

	// first we assemble the boundary nodes. First the nodes in row 0 (the south boundary)
	for (int k = 0; k<NCols; k++)
	{
		ins[k][k] << 1.0;
		mtl_b_vector[k] =  South_boundary_elevation;//zeta_last_timestep[0][k];
	}

	// now assemble the north boundary
	int starting_north_boundary = (NRows+1)*(NCols);
	int one_past_last_north_boundary = (NRows+2)*NCols;
	for (int k = starting_north_boundary; k < one_past_last_north_boundary; k++)
	{
		ins[k][k] << 1.0;
		mtl_b_vector[k] = North_boundary_elevation;//zeta_last_iter[NCols-1][k];
	}
	
	// create the zeta matrix that includes the boundary conditions
	Array2D<float> zeta_for_implicit(NRows+2, NCols,0.0);
	for (int col = 0; col<NCols; col++)
	{
		zeta_for_implicit[0][col] = zeta_last_iter[0][col];
		zeta_for_implicit[NRows+1][col] = zeta_last_iter[NRows-1][col];
	}
	for (int row = 0; row<NRows; row++)
	{
		for (int col = 0; col<NCols; col++)
		{
			zeta_for_implicit[row+1][col] = zeta_last_iter[row][col];
		}
	}

	// now assemble the rest
	// we loop through each node
	int counter = 0;
	float b_value;
	int k_value_i_j,k_value_ip1_j,k_value_im1_j,k_value_i_jp1,k_value_i_jm1;
	for (int row = 0; row<NRows; row++)
	{
		for (int col = 0; col<NCols; col++)
		{
			if  (col == 0 || col == NCols-1)
	    {
				b_value = zeta_last_iter[row][col];
			}
			else
	    {
				b_value = zeta_last_timestep[row][col]+dt*uplift_rate[row][col]-dt*fluvial_erosion_rate[row][col];
			}
			k_value_ip1_j = vec_k_value_ip1_j[counter];
			k_value_im1_j = vec_k_value_im1_j[counter];
			k_value_i_j   = vec_k_value_i_j[counter];
			k_value_i_jp1 = vec_k_value_i_jp1[counter];
			k_value_i_jm1 = vec_k_value_i_jm1[counter];

			A =  dy_front_term/(1 -
			        (zeta_for_implicit[row+2][col]-zeta_for_implicit[row+1][col])*
			        (zeta_for_implicit[row+2][col]-zeta_for_implicit[row+1][col])*
					inv_dy_S_c_squared);
			B = dy_front_term/(1 -
			        (zeta_for_implicit[row+1][col]-zeta_for_implicit[row][col])*
			        (zeta_for_implicit[row+1][col]-zeta_for_implicit[row][col])*
					inv_dy_S_c_squared);

			// logic for west periodic boundary
			if(col == 0)
			{
				C = dx_front_term/(1 -
			           (zeta_for_implicit[row+1][col+1]-zeta_for_implicit[row+1][col])*
			           (zeta_for_implicit[row+1][col+1]-zeta_for_implicit[row+1][col])*
					   inv_dx_S_c_squared);
				D = dx_front_term/(1 -
			           (zeta_for_implicit[row+1][col]-zeta_for_implicit[row+1][NCols-1])*
			           (zeta_for_implicit[row+1][col]-zeta_for_implicit[row+1][NCols-1])*
					   inv_dx_S_c_squared);
 				//A=B=C=D=0;
			}
			// logic for east periodic boundary
			else if(col == NCols-1)
			{
				C = dx_front_term/(1 -
			           (zeta_for_implicit[row+1][0]-zeta_for_implicit[row+1][col])*
			           (zeta_for_implicit[row+1][0]-zeta_for_implicit[row+1][col])*
					   inv_dx_S_c_squared);
				D = dx_front_term/(1 -
			           (zeta_for_implicit[row+1][col]-zeta_for_implicit[row+1][col-1])*
			           (zeta_for_implicit[row+1][col]-zeta_for_implicit[row+1][col-1])*
					   inv_dx_S_c_squared);
				//A=B=C=D=0;

			}
			// logic for rest of matrix
			else
			{
				C = dx_front_term/(1 -
			           (zeta_for_implicit[row+1][col+1]-zeta_for_implicit[row+1][col])*
			           (zeta_for_implicit[row+1][col+1]-zeta_for_implicit[row+1][col])*
					   inv_dx_S_c_squared);

				D = dx_front_term/(1 -
			           (zeta_for_implicit[row+1][col]-zeta_for_implicit[row+1][col-1])*
			           (zeta_for_implicit[row+1][col]-zeta_for_implicit[row+1][col-1])*
					   inv_dx_S_c_squared);

			}

			// place the values in the assembly matrix and the b vector
			mtl_b_vector[k_value_i_j] = b_value;
			ins[k_value_i_j][k_value_ip1_j] << -A;
			ins[k_value_i_j][k_value_im1_j] << -B;
			ins[k_value_i_j][k_value_i_jp1] << -C;
			ins[k_value_i_j][k_value_i_jm1] << -D;
			ins[k_value_i_j][k_value_i_j] << 1+A+B+C+D;

			counter++;
		}
	}
}

//------------------------------------------------------------------------------
// mtl_solve_assembler_matrix
// this function assembles the solution matrix
//------------------------------------------------------------------------------
void LSDRasterModel::mtl_solve_assembler_matrix(Array2D<float>& zeta_last_iter, Array2D<float>& zeta_last_timestep,
						 Array2D<float>& zeta_this_iter, Array2D<float>& uplift_rate, Array2D<float>& fluvial_erosion_rate,
						 float dt, int problem_dimension, float inv_dx_S_c_squared, float inv_dy_S_c_squared,
					   float dx_front_term, float dy_front_term,
	           vector<int>& vec_k_value_i_j, vector<int>& vec_k_value_ip1_j, vector<int>& vec_k_value_im1_j,
	           vector<int>& vec_k_value_i_jp1, std::vector<int>& vec_k_value_i_jm1,
	           float South_boundary_elevation, float North_boundary_elevation)
{
	// reset the zeta array for this iteration
	Array2D<float> empty_zeta(NRows,NCols,0.0);
	zeta_this_iter = empty_zeta.copy();
	//zeta_this_iter(NRows, NCols, XMinimum, YMinimum, DataResolution, NoDataValue, empty_zeta);
	// create a mtl matrix
	// NOTE: you could probably save time by creating the mtl matrix and vector
	// in main()
	mtl::compressed2D<float> mtl_Assembly_matrix(problem_dimension, problem_dimension);
	mtl::dense_vector<float> mtl_b_vector(problem_dimension,0.0);

	// assemble the matrix
	mtl_assemble_matrix(zeta_last_iter, zeta_last_timestep, zeta_this_iter, 
						uplift_rate, fluvial_erosion_rate, mtl_Assembly_matrix, mtl_b_vector,
						dt, problem_dimension, inv_dx_S_c_squared, inv_dy_S_c_squared,
						dx_front_term, dy_front_term, South_boundary_elevation, North_boundary_elevation,
						vec_k_value_i_j, vec_k_value_ip1_j, vec_k_value_im1_j, vec_k_value_i_jp1, vec_k_value_i_jm1);
	//std::cout << "matrix assembled!" << endl;

	//std::ofstream assembly_out;
	//assembly_out.open("assembly.data");
	//assembly_out << mtl_Assembly_matrix << endl;
	//assembly_out.close();

	// now solve the mtl system
	// Create an ILU(0) preconditioner
	long time_start, time_end, time_diff;
	time_start = time(NULL);
	itl::pc::ilu_0< mtl::compressed2D<float> > P(mtl_Assembly_matrix);
	mtl::dense_vector<float> mtl_zeta_solved_vector(problem_dimension);
	itl::basic_iteration<float> iter(mtl_b_vector, 500, 1.e-8);
	bicgstab(mtl_Assembly_matrix, mtl_zeta_solved_vector, mtl_b_vector, P, iter);
	time_end = time(NULL);
	time_diff = time_end-time_start;
	//std::cout << "iter MTL bicg took: " << time_diff << endl;

	// now reconstitute zeta
	int counter = 0;//NCols;
	for (int row = 0; row<NRows; row++)
	{
		for (int col = 0; col < NCols; col++)
		{
			zeta_this_iter[row][col] = mtl_zeta_solved_vector[counter];
			counter++;
		}
	}
}

//------------------------------------------------------------------------------
// nonlinear_creep_timestep
// do a creep timestep.  This function houses the above two functions to
// undertake model timestep using implicit implementation of the nonlinear
// transport law.
// NOTE you need to run mtl_initiate_assembler_matrix before you run this function
//------------------------------------------------------------------------------
void LSDRasterModel::nonlinear_creep_timestep(Array2D<float>& fluvial_erosion_rate, float iteration_tolerance,
			int problem_dimension, float inv_dx_S_c_squared, float inv_dy_S_c_squared,
			float dx_front_term, float dy_front_term, vector<int>& vec_k_value_i_j,
			vector<int>& vec_k_value_ip1_j, vector<int>& vec_k_value_im1_j,
			vector<int>& vec_k_value_i_jp1,	vector<int>& vec_k_value_i_jm1,
			float South_boundary_elevation, float North_boundary_elevation)
{
	 // reset zeta_old and zeta_intermediate
	Array2D<float> zeta = RasterData.copy();  
	Array2D<float> zeta_old = zeta.copy();
	Array2D<float> zeta_intermediate = zeta.copy();

	// set up residual
	float residual;
	float N_nodes = float(NRows*NCols);
	int iteration = 0;
	int Max_iter = 100;
	do
	{
		residual = 0.0;
		mtl_solve_assembler_matrix(zeta, zeta_old, zeta_intermediate,
				uplift_field, fluvial_erosion_rate, timeStep,
				problem_dimension, inv_dx_S_c_squared, inv_dy_S_c_squared,
				dx_front_term, dy_front_term, vec_k_value_i_j, vec_k_value_ip1_j,
				vec_k_value_im1_j, vec_k_value_i_jp1, vec_k_value_i_jm1,
				South_boundary_elevation, North_boundary_elevation);


		// check the residuals (basically this is the aveage elevation change between intermediate
		// zeta values
		for (int row = 0; row<NRows; row++)
		{
			for (int col = 0; col<NCols; col++)
			{
				residual+= sqrt( (zeta_intermediate[row][col]-zeta[row][col])*
								 (zeta_intermediate[row][col]-zeta[row][col]) );
			}
		}
		residual = residual/N_nodes;

		// reset last zeta
		zeta = zeta_intermediate.copy();
		
		iteration++;

		if (iteration%5 == 0)
		{
			//std::cout << "iteration is: " << iteration << " and residual RMSE is: " << residual << endl;
		}
		if (iteration > Max_iter)
		{
			iteration_tolerance = iteration_tolerance*10;
			iteration = 0;
		}
	} while (residual > iteration_tolerance);

	//   LSDRasterModel ZetaNew(NRows, NCols, XMinimum, YMinimum, DataResolution, NoDataValue, zeta);
	// Update Raster Data
	RasterData = zeta.copy();
}


//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// RUN MODEL
//------------------------------------------------------------------------------
// A series of wrapper functions that implement the numerical model
//------------------------------------------------------------------------------
// implicit_hillslope_and_fluvial
// This function sets up a landscape evolution model run incorporating fluvial
// erosion and hillslope erosion via non-linear creep.  It calls the implicit
// implementation.
// The user should provide the parameter file which sets out the details of the 
// model run.   
//------------------------------------------------------------------------------
LSDRasterModel LSDRasterModel::run_model_implicit_hillslope_and_fluvial(string param_file)
{
	// parameters. All of these are initialized using the .param file
	// time and printing paramters
	float dt = timeStep;	      // time spacing
	float EndTime = endTime;		      // the time at which the model ends
	float PrintInterval;     // the frequency at which the model prints data

	// fluvial parameters
	float k_w;					      // parameter for determining channel width from discharge in m^3/s
								            // based in Emmett (1975) as cited in Knighton (1988)from Salmon River, ID
	float b;					        // channel width exponent.
	float m;					        // area exponent for fluvial incision
	float n;					        // slope exponent for fluvial incision
	float K;					        // coefficient for fluvial incision
	float ErosionThreshold;	// threshold amunt of fluvial action required to erode bed (at this stage
								            // it is an erosion rate subtracted from the main stream power erosion rate)

	// creep-like parameters
	float K_nl;				// diffusivity of hillslope sediment
	float S_c;					// critical slope

	// forcing parameters
	float uplift_rate;			// rate of rock uplift
	float precip_rate;			// rate of precipitation

	// boundary_conditions
	float North_boundary_elevation;		// the elevation of the channels at the north
	float South_boundary_elevation;		// and south bounding nodes. Note these nodes only appear
											                // in the buffered grid
	// file names
	string run_name;			        // the name of the run
							// the file formats are: file_type.run_name.time_step.data
	//string surf_fname;			        // the name of the surface file
	string area_name;
	string div_name;
	string w_name;
	string erosion_rate_fname;

	// data elements: vectors and arrays
	LSDRasterModel Zeta(NRows, NCols, XMinimum, YMinimum, DataResolution, NoDataValue, get_RasterData());
	Array2D<float> ZetaOld;				    // surface from last timestep (for getting sediment flux)
	Array2D<float> ZetaTemp(NRows,NCols);
	Array2D<float> ZetaDivergence;		  // del dot zeta
	Array2D<float> PrecipitationFlux;	// precipitation flux (will be in units m^3 per second
	Array2D<float> Q_w;					      // discahrge (in m^3/second)
	Array2D<float> w;						      // channel width in m
	Array2D<float> FluvialErosionRate;	// the fluvial erosion rate in m/yr
	Array2D<float> SlopesBetweenCols;
	Array2D<float> SlopesBetweenRows;
	Array2D<float> ErosionRate;

	// file for printing timesteps
	ofstream ts_out;
	cout << "LINE " << __LINE__ << ": Initializing model" << endl;

	// initiate the model
	Zeta.initialize_model(param_file, run_name, dt, EndTime, PrintInterval,
	  k_w, b, m, n, K, ErosionThreshold, K_nl, S_c, uplift_rate, precip_rate,
	  North_boundary_elevation, South_boundary_elevation, 
	  PrecipitationFlux, SlopesBetweenRows, SlopesBetweenCols, ErosionRate);
	  
	cout << "LINE " << __LINE__ << ": Model initialized" << endl;
	Array2D<float> UpliftRate(NRows,NCols,uplift_rate);

	// print the initial condition
	float t_ime = 0;
	Array2D<float> Q_w_temp(NRows,NCols,0.0);
	Q_w = Q_w_temp.copy();
	//int print_counter = 0;
// 	print_at_print_interval(run_name, t_ime, dt, N_rows, N_cols, print_interval,           // NEED TO CHANGE THIS MODULE SO THAT IT SIMPLY CALLS LSDRasterModel.write_raster()
// 								print_counter, ts_out, zeta, erosion_rate, fluvial_erosion_rate, Q_w);

//	cout << "initial_condition printed " << endl;

	// now use the tolerance method
	// initialize some variables needed to speed up the calucaltions
	int problem_dimension;
	float inv_dx_S_c_squared, inv_dy_S_c_squared, dx_front_term, dy_front_term;
	vector<int> vec_k_value_i_j;
	vector<int> vec_k_value_ip1_j;
	vector<int> vec_k_value_im1_j;
	vector<int> vec_k_value_i_jp1;
	vector<int> vec_k_value_i_jm1;
	float iteration_tolerance = 0.01;

	// now initialize the assembly matrix and some constants
	Zeta.mtl_initiate_assembler_matrix(problem_dimension,				     
				     inv_dx_S_c_squared, inv_dy_S_c_squared, dx_front_term, dy_front_term, 
	             vec_k_value_i_j, vec_k_value_ip1_j, vec_k_value_im1_j, vec_k_value_i_jp1, vec_k_value_i_jm1);

	if (not quiet) cout << "LINE " << __LINE__ << ": assembler matrix initialized" << endl;
	
	// do a time loop
	// now do the time loop
	while (t_ime < EndTime)
	{
		t_ime+= dt;

		if (not quiet) cout << flush << "time is: " << t_ime << "\r";
	  
// 		// buffer the landscape
// 		ZetaBuff = Zeta.create_buffered_surf(South_boundary_elevation,North_boundary_elevation);
//     
//     // Fill Buffered array
//     float min_slope = 0.0001;
//     ZetaBuff.fill_overwrite(min_slope);   
//     Array2D<float> ZetaUpdate(NRows,NCols);
//     // now update original surface
//     for (int i = 0; i<NRows; ++ i)
//     {
//       for (int j = 0; j<NCols; ++j)
//       {
//         ZetaUpdate[i][j]=ZetaBuff.get_data_element(i+1,j+1);
//       }
//     }
//     
//     Zeta.RasterData = ZetaUpdate;
	  
		// ERODE FLUVIALLY USING FASTSCAPE
		// Do flow routing using FlowInfo to calculate Q_w
		
	  // now get the channel widths
		//w = array_channel_width_wolman(Q_w, k_w, b);

		// calcualte the topographic divergence
		//ZetaDivergence = Zeta.get_topographic_divergence();    
	  
	  // calculate fluvial erosion rate based on Fastscape
	  //FluvialErosionRate = calculate_fluvial_erosion_rate(w, Q_w, ZetaDivergence, K, n, m, ErosionThreshold);	
	  Array2D<float> fluvial_temp(NRows,NCols,0.0);
	  //FluvialErosionRate = fluvial_temp.copy;
	  // update elevations to reflect fluvial incision during timestep?
	  // - we are going to need to think about this if we are to maintain stable hillslopes
	  
	// HILLSLOPE EROSION
	  // do a nonlinear creep timestep
//    cout << "/n non_linear_creep_timestep" << endl;
	  Zeta.nonlinear_creep_timestep(fluvial_temp,
					iteration_tolerance, problem_dimension,
					inv_dx_S_c_squared, inv_dy_S_c_squared, dx_front_term, dy_front_term,
					vec_k_value_i_j, vec_k_value_ip1_j, vec_k_value_im1_j,
					vec_k_value_i_jp1, vec_k_value_i_jm1, 
	          South_boundary_elevation, North_boundary_elevation);
	                      
	  // calculate erosion rate
//    ErosionRate = Zeta.calculate_erosion_rates(ZetaOld,dt);

//		print_at_print_interval(run_name, t_ime, dt, NRows, NCols, print_interval,
//								print_counter, ts_out, zeta, erosion_rate, fluvial_erosion_rate, Q_w);
	}
	ts_out.close();
	//LSDRasterModel(NRows, NCols, XMinimum, YMinimum, DataResolution, NoDataValue, Zeta.get_RasterData());
	return Zeta;
}

void LSDRasterModel::run_components( void )
{
	//recording = false;
	cycle_number = 1;
	total_erosion = 0;
	max_erosion = 0;
	min_erosion = -99;
	switch_delay = 0;
	time_delay = 0;

	stringstream ss, ss_root;
	int frame = 1, print = 1;
	do {
		// Check if hung
		if ( check_if_hung() )
		{
			cout << "Model took too long to reach steady state, assumed to be stuck" << endl;
			break;
		}
		check_periodicity_switch();
		// Record current topography
		zeta_old = RasterData.copy();
		// run active model components
		
		if (hillslope)
		{
			if (nonlinear)
				soil_diffusion_fv_nonlinear();
			else
				soil_diffusion_fd_linear();
		}

		wash_out();
		if (fluvial)
			fluvial_incision();
		if (isostasy)
		{
			if (flexure)
				//flexural_isostasy( 0.00001 );
				flexural_isostasy_alt( );
			else
				Airy_isostasy();
		}

		uplift_surface();
		write_report();

		current_time += timeStep;

		if ( print_interval > 0 && (print % print_interval) == 0)	// write at every print interval
		{
			print_rasters( frame );
			++frame;
		}
		if (not quiet) cout << "\rTime: " << current_time << " years" << flush;
		++print;
		check_steady_state();
	} while (not check_end_condition());
	if ( print_interval == 0 || (print_interval > 0 && ((print-1) % print_interval) != 0))
	{
		//if (not quiet) cout << current_time << ": " << endl;
		print_rasters( frame );
	}
}

void LSDRasterModel::run_model( void )
{
	// file to write details of each time step
	// This will be opened by the write_report method
	int run = 1;
	do {
	initial_steady_state = false;
	recording = false;

	if (not initialized && not quiet)
	{
		cout << "Model has not been initialized with a parameter file." << endl;
		cout << "All values used are defaults" << endl;
	}

		current_time = 0;
		run_components();
	++run;
	} while (run <= num_runs);
	
	final_report();
}

void LSDRasterModel::run_model_from_steady_state( void )
{
	// file to write details of each time step
	// This will be opened by the write_report method
	RasterData = steady_state_data;
	reset_model();

	if (not initialized && not quiet)
	{
		cout << "Model has not been initialized with a parameter file." << endl;
		cout << "All values used are defaults" << endl;
	}
	if (not initial_steady_state)
	{
		cout << "Model has not been set to steady state yet" << endl;
		cout << "Run LSDRasterModel::reach_steady_state( float tolerance ) first" << endl;
	}
	// Generate random noise

	int run = 1;
	stringstream ss, ss_root;
	do{
		current_time = 0;
		run_components();		
		++run;
	} while (run <= num_runs);

	// If the last output wasn't written, write it
	
	if (not quiet) cout << "\nModel finished!\n" << endl;
	final_report();
}

void LSDRasterModel::reach_steady_state( void )
{
	// file to write details of each time step
	// This will be opened by the write_report method
	initial_steady_state = false;
	current_time = 0;
	total_erosion = 0;
	max_erosion = 0;
	min_erosion = -99;

	int K_mode_swap = K_mode;
	int D_mode_swap = D_mode;
	float K_amp_swap = K_amplitude;
	float endTime_swap = endTime;
	float period_swap = periodicity;
	int period_mode_swap = period_mode;
	float print_interval_swap = print_interval;
	bool reporting_swap = reporting;
	string name_swap = name;

	if (not initialized && not quiet)
	{
		cout << "Model has not been initialized with a parameter file." << endl;
		cout << "All values used are defaults" << endl;
	}
	// Generate random noise
	random_surface_noise(0, noise);
	// Fill the topography
	LSDRaster *temp;
	temp = new LSDRaster(*this);
  float thresh_slope = 0.00001;
  *temp = fill(thresh_slope);
	RasterData = temp->get_RasterData();
	delete temp;

	// Run model with some modest fluvial forcing
	K_mode = 1;
	K_amplitude = K_fluv * 0.3;
	endTime = 0;
	//periodicity = 2000;
	period_mode = 1;
	cycle_steady_check = true;
	print_interval = 0;
	reporting = false;

	if (not quiet) cout << "Producing steady state profile" << endl;
	run_components();

	// Now run with static forcing
	K_mode = K_mode_swap;
	D_mode = D_mode_swap;
	K_amplitude = K_amp_swap;
	endTime = timeStep*10;
	cycle_steady_check = false;
	initial_steady_state = false;
	current_time = 0;

	if (not quiet) cout << "Producing steady state elevation of base level forcing" << endl;
	run_components();

	endTime = endTime_swap;
	periodicity = period_swap;
	period_mode = period_mode_swap;
	cycle_steady_check = false;
	print_interval = print_interval_swap;
	reporting = reporting_swap;

	steady_state_data = Array2D<float> (NRows, NCols, 0.0);
	steady_state_data = RasterData;
}

void LSDRasterModel::reset_model( void )
{
	total_erosion = 0;
	total_response = 0;
}

void LSDRasterModel::soil_diffusion_fv( void )
{
	static bool defined = false;
	
	static int problem_dimension;
	static float inv_dx_S_c_squared, inv_dy_S_c_squared, dx_front_term, dy_front_term;
	static vector<int> vec_k_value_i_j;
	static vector<int> vec_k_value_ip1_j;
	static vector<int> vec_k_value_im1_j;
	static vector<int> vec_k_value_i_jp1;
	static vector<int> vec_k_value_i_jm1;
	static float iteration_tolerance = 0.01;

	Array2D <float> fluvial_temp(NRows, NCols, 0.0);
	float south, north;

	if (boundary_conditions[2][0] == 'b')
	{
		south = 0.0;
		north = current_time*get_max_uplift();	

		cout << south << ", " << north << endl;
	}
	else
	{
		cout << "Model currently not built to cope with hillslope diffusion using these boundary conditions" << endl;
		cout << "Feature implementation required" << endl;
		exit(1);
	}

	
	if (not defined)
	{
	mtl_initiate_assembler_matrix(problem_dimension, inv_dx_S_c_squared, inv_dy_S_c_squared, 
	                dx_front_term, dy_front_term, vec_k_value_i_j, vec_k_value_ip1_j,
			vec_k_value_im1_j, vec_k_value_i_jp1, vec_k_value_i_jm1);
	}
	defined = true;

	nonlinear_creep_timestep(fluvial_temp, iteration_tolerance, problem_dimension,
			inv_dx_S_c_squared, inv_dy_S_c_squared,	dx_front_term, dy_front_term, vec_k_value_i_j,
			vec_k_value_ip1_j, vec_k_value_im1_j, vec_k_value_i_jp1, vec_k_value_i_jm1, 
			south, north);
}

void LSDRasterModel::interpret_boundary(short &dimension, bool &periodic, int &size)
{
	dimension = 0;
	for (int i=0; i<4; ++i)
	{
		if (boundary_conditions[i][0] == 'b')
			dimension = i % 2;
	}
	if (boundary_conditions[1-dimension][0] == 'p' || boundary_conditions[3-dimension][0] == 'p')
	{
		periodic = true;
		if (not (boundary_conditions[1-dimension][0] && boundary_conditions[3-dimension][0] == 'p'))
			if (not quiet) cout << "Warning! Entered one boundary as periodic, but not t'other! Assuming both are periodic." << endl;
	}
	if (dimension == 0)
		size = (NRows-2)*NCols;
	else
		size = NRows*(NCols-2);

	if (dimension != 0 && dimension != 1)
	{
		cerr << "Warning line " << __LINE__ << ": Variable 'dimension' should have a value of 0 or 1" << endl;
		exit(1);
	}
}


mtl::compressed2D<float> LSDRasterModel::generate_fd_matrix( int dimension, int size, bool periodic )
{
	int num_neighbours, num_neighbours_;
	int row, col;
	float r = get_D() * timeStep / (DataResolution * DataResolution);
	float r_ = get_D() * timeStep / pow(DataResolution * 1.4142135623, 2);
	int width, height;

	if (dimension == 0)		// North - south
	{
		width = NCols;
		height = NRows - 2;
	}
	else					// East - west
	{
		width = NCols - 2;
		height = NRows;
	}

	mtl::compressed2D<float> matrix(size, size);
	matrix = 0.0;
	mtl::matrix::inserter< mtl::compressed2D<float> > ins(matrix);

	for (int i=0; i<size; ++i)
	{
		row = i / width;
		col = i % width;
		num_neighbours = 4;				
		num_neighbours_ = 4;
		// left
		if (col > 0)
		{
			ins[i][i-1] << -r;
		}
		else if (dimension == 0)
		{
			if (not periodic)
				--num_neighbours;
			else
				ins[i][i+width-1] << -r;
		}

		// right
		if (col < width - 1)
		{
			ins[i][i+1] << -r;
		}
		else if (dimension == 0)
		{
			if (not periodic)
				--num_neighbours;
			else
				ins[i][i-width+1] << -r;
		}

		// up
		if (row > 0)
		{
			ins[i][i-width] << -r;
		}
		else if (dimension == 1)
		{
			if (not periodic)
				--num_neighbours;
			else
				ins[i][i+(width*(NCols-1))] << -r;
		}

		// down
		if (row < height-1)
		{
			ins[i][i+width] << -r;
		}
		else if (dimension == 1)
		{
			if (not periodic)
				--num_neighbours;
			else
				ins[i][i-(width*(NCols-1))] << -r;
		}

		// Diagonals
		// Upper left
		if (row > 0 && col > 0)
			ins[i][i-width-1] << -r_;
		else if (dimension == 0 && row > 0)
			if (not periodic)
				--num_neighbours_;
			else{
				ins[i][i-1] << -r_;}
		else if (dimension == 1 && col > 0)
		{
			if (not periodic)
				--num_neighbours_;
			else
				ins[i][i+(width*(NCols-1))-1] << -r;
		}


		// Upper right
		if (row > 0 && col < width-1)
			ins[i][i-width+1] << -r_;
		else if (dimension == 0 && row > 0)
		{
			if (not periodic)
				--num_neighbours_;
			else
				ins[i][i-(2*width)+1] << -r_;
		}
		else if (dimension == 1 && col < width-1)
		{
			if (not periodic)
				--num_neighbours_;
			else
				ins[i][i+(width*(NCols-1))+1] << -r_;
		}
		
		// Lower left
		if (row < height-1 && col > 0)
			ins[i][i+width-1] << -r_;
		else if (dimension == 0 && row < height-1)
		{
			if (not periodic)
				--num_neighbours_;
			else
				ins[i][i+(2*width)-1] << -r_;
		}

		else if (dimension == 1 && col > 0)
		{
			if (not periodic)
				--num_neighbours_;
			else
				ins[i][col-1] << -r_;
		}

		// Lower right
		if (row < height-1 && col < width-1)
		{
			ins[i][i+width+1] << -r_;
		}
		else if (dimension == 0 && row < height-1)
		{
			if (not periodic)
				--num_neighbours_;
			else
				ins[i][i+1] << -r_;
		}

		else if (dimension == 1 && col < width-1)
		{
			if (not periodic)
				--num_neighbours_;
			else
				ins[i][col+1] << -r_;
		}

		ins[i][i] << num_neighbours*r + 1 + num_neighbours_ * r_;
	}
	return matrix;
}

mtl::dense_vector <float> LSDRasterModel::build_fd_vector(int dimension, int size)
{
	int vector_pos = 0;
	mtl::dense_vector <float> data_vector(size);
	float push_val;
	float r = get_D() * timeStep / (DataResolution * DataResolution);
	int start_i, end_i;
	int start_j, end_j;

	if (dimension == 0)
	{
		start_i = 1; end_i = NRows-2;
		start_j = 0; end_j = NCols-1;
	}
	else 			// East - west
	{
		start_i = 0; end_i = NRows-1;
		start_j = 1; end_j = NCols-2;
	}

	for (int i=start_i; i<=end_i; ++i)
	{
		for (int j=start_j; j<=end_j; ++j)
		{
			push_val = RasterData[i][j];
			if (dimension == 0)
			{
				if (i==1)
					push_val += RasterData[0][j] * r;
				else if (j==NRows-2)
					push_val += RasterData[NRows-1][j] * r;
			}

			else if (dimension == 1)
			{
			if (j == 1)
				push_val += RasterData[i][0] * r;
			else if (j == NCols-2)
				push_val += RasterData[i][NCols-1] * r;
			}

			data_vector[vector_pos] = push_val;
			++vector_pos;
		}
	}
	
	return data_vector;
}

/*
void LSDRasterModel::repack_fd_vector(mtl::dense_vector <float> &data_vector, int dimension)
{
	int vector_pos = 0;
	if (dimension == 1) 			// East - west
	{
		for (int i=0; i<NRows; ++i)
		{
			for (int j=1; j<NCols-1; ++j)
			{
				RasterData[i][j] = data_vector[vector_pos];
				++vector_pos;
			}
		}
	}
	if (dimension == 0) 			// North - south
	{
		for (int j=0; j<NCols; ++j)
		{
			for (int i=1; i<NRows-1; ++i)
			{
				RasterData[i][j] = data_vector[vector_pos];
				++vector_pos;
			}
		}
	}
}
*/

void LSDRasterModel::soil_diffusion_fd_linear( void )
{
	short dimension;
	bool periodic;
	int size;

	interpret_boundary(dimension, periodic, size);
	//cout << "Periodic " << periodic << endl;
	
	mtl::compressed2D <float> matrix = generate_fd_matrix(dimension, size, periodic);
	// Unpack data
	mtl::dense_vector <float> data_vector = build_fd_vector(dimension, size);

	if (not quiet && name == "debug" && size < 100)
	{
		cout << "Data: " << endl;
		for (int i=0; i<NRows; ++i)
		{
			for (int j=0; j<NCols; ++j)
			{
				cout << RasterData[i][j] << " ";
			}
			cout << endl;
		}
		cout << "Matrix: " << endl;
		for (int i=0; i<size; ++i)
		{
			for (int j=0; j<size; ++j)
			{
				cout << matrix[i][j] << " ";
			}
			cout << endl;
		}
		cout << "Vector: " << endl;
		for (int i=0; i<size; ++i)
			cout << data_vector[i] << endl;
	}

	mtl::dense_vector <float> output(size);
	// Set up preconditioner
	itl::pc::ilu_0 < mtl::compressed2D<float> > P(matrix);
	// Iterator condition
	itl::basic_iteration <float> iter(data_vector, 200, 1e-6);
	// Matrix solver
	itl::bicgstab(matrix, output, data_vector, P, iter);

	
	repack_vector(output, dimension);
	if (not quiet && name == "debug" && size <100)
	{
		cout << "Output: " << endl;
		for (int i=0; i<size; ++i)
			cout << output[i] << endl;
		for (int i=0; i<NRows; ++i)
		{
			for (int j=0; j<NCols; ++j)
			{
				cout << RasterData[i][j] << " ";
			}
			cout << endl;
		}
	}
}

mtl::compressed2D<float> LSDRasterModel::generate_fv_matrix( int dimension, int size, bool periodic )
{
	float A, B, C, D;
	float front = timeStep * get_D() / (DataResolution*DataResolution);
	float inv_term = 1 / (DataResolution * DataResolution * S_c * S_c);

	int p = 0;	// positioner for matrix insertion
	int offset;
	int start_i, start_j, end_i, end_j;
	mtl::compressed2D <float> matrix(size, size);
	matrix = 0.0;
	mtl::matrix::inserter< mtl::compressed2D<float> > ins(matrix);
	
	if (dimension == 0)
	{
		start_i = 1; end_i = NRows-2;
		start_j = 0; end_j = NCols-1;
		offset = NCols;
	}
	else 
	{
		start_i = 0; end_i = NRows-1;
		start_j = 1; end_j = NCols-2;
		offset = NCols - 2;
	}
	
	for (int i=start_i; i<=end_i; ++i)
	{
		for (int j=start_j; j<=end_j; ++j)
		{
			A = (i==0)		? 0 : front / (1 - pow(RasterData[i][j] - RasterData[i-1][j], 2) * inv_term);
			B = (j==NCols-1)	? 0 : front / (1 - pow(RasterData[i][j] - RasterData[i][j+1], 2) * inv_term);
			C = (i==NRows-1) 	? 0 : front / (1 - pow(RasterData[i][j] - RasterData[i+1][j], 2) * inv_term);
			D = (j==0)		? 0 : front / (1 - pow(RasterData[i][j] - RasterData[i][j-1], 2) * inv_term);

			if (periodic)
			{
				if (i==0) 		A = front / (1 - pow(RasterData[i][j] - RasterData[NRows-1][j], 2) * inv_term);
				else if (j==NCols-1) 	B = front / (1 - pow(RasterData[i][j] - RasterData[i][0], 2) * inv_term);
				else if (i==NRows-1) 	C = front / (1 - pow(RasterData[i][j] - RasterData[0][j], 2) * inv_term);
				else if (j==0) 		D = front / (1 - pow(RasterData[i][j] - RasterData[i][NCols-1], 2) * inv_term);

			}
			
			ins[p][p] << 1 + A + B + C + D;
			if (j != start_j)
				ins[p][p-1] << -D;
			else if (periodic && dimension == 0 )
				ins[p][p+offset-1] << -D;
			if (j != end_j)
				ins[p][p+1] << -B;
			else if (periodic && dimension == 0 )
				ins[p][p-offset+1] << -B;
			if (i != start_i)
				ins[p][p-offset] << -A;
			else if (periodic && dimension == 1 )
				ins[p][p+(offset*(NCols-1))] << -A;
			if (i != end_i)
				ins[p][p+offset] << -C;
			else if (periodic && dimension == 1 )
				ins[p][p-(offset*(NCols-1))] << -C;

			++p;
		}
	}
	return matrix;
}

mtl::dense_vector <float> LSDRasterModel::build_fv_vector( int dimension, int size )
{	
	float front = timeStep * get_D() / (DataResolution*DataResolution);
	float inv_term = 1 / (DataResolution * DataResolution * S_c * S_c);

	mtl::dense_vector <float> data_vector(size);
	int p = 0;		// vector positioner
	int start_i, end_i;
	int start_j, end_j;
	float push_val;

	if (dimension == 0)
	{
		start_i = 1; end_i = NRows-2;
		start_j = 0; end_j = NCols-1;
	}
	else 
	{
		start_i = 0; end_i = NRows-1;
		start_j = 1; end_j = NCols-2;
	}

	for (int i=start_i; i<=end_i; ++i)
	{
		for (int j=start_j; j<=end_j; ++j)
		{
			push_val = zeta_old[i][j];
//			cout << push_val << endl;

			if (dimension == 0)
			{
				if (i==1)
					push_val += zeta_old[0][j] * front / 
						(1 - pow(RasterData[i][j]-RasterData[0][j],2) * inv_term);
				if (i==NRows-2)
					push_val += zeta_old[NRows-1][j] * front / 
						(1 - pow(RasterData[i][j]-RasterData[NRows-1][j],2) * inv_term);
			}
			else if (dimension == 1)
			{
				if (j==1)
					push_val += zeta_old[i][0] * front / 
						(1 - pow(RasterData[i][j]-RasterData[i][0],2) * inv_term);
				if (j==NCols-2)
					push_val += zeta_old[i][NCols-1] * front / 
						(1 - pow(RasterData[i][j]-RasterData[i][NCols-1],2) * inv_term);
			}

			data_vector[p] = push_val;
			++p;
		}
	}
	return data_vector;
}

void LSDRasterModel::repack_vector(mtl::dense_vector <float> &data_vector, int dimension) 
{
	int start_i, end_i;
	int start_j, end_j;
	int p = 0;

	if (dimension == 0)
	{
		start_i = 1; end_i = NRows-2;
		start_j = 0; end_j = NCols-1;
	}
	else
	{
		start_i = 0; end_i = NRows-1;
		start_j = 1; end_j = NCols-2;
	}

	for (int i = start_i; i<=end_i; ++i)
	{
		for (int j = start_j; j<=end_j; ++j)
		{
			RasterData[i][j] = data_vector[p];
			++p;
		}
	}
}


void LSDRasterModel::soil_diffusion_fv_nonlinear( void )
{
	int max_iter = 200, iter = 0;
	float epsilon = 0.00001;
	float max_diff;
	Array2D <float> last_iteration;

	short dimension = 0;
	bool periodic;
	int size;
	interpret_boundary(dimension, periodic, size);

	do {
	last_iteration = RasterData.copy();
	// A
	mtl::compressed2D <float> matrix = generate_fv_matrix(dimension, size, periodic);
	// b
	mtl::dense_vector <float> data_vector = build_fv_vector(dimension, size);
	
	if (not quiet && name == "debug" && NRows <= 10 && NCols <= 10)
	{
	cout << "Data: " << endl;
	for (int i=0; i<NRows; ++i)
	{
		for (int j=0; j<NCols; ++j)
		{
			cout << RasterData[i][j] << " ";
		}
		cout << endl;
	}
	cout << "Matrix: " << endl;
	for (int i=0; i<size; ++i)
	{
		for (int j = 0; j<size; ++j)
		{
			cout <<matrix[i][j] << " ";
		}
		cout << endl;
	}
	cout << "Vector: " << endl;
	for (int i=0; i<size; ++i)
		cout << data_vector[i] << endl;
	}

	// x
	mtl::dense_vector <float> output(size);
	// Set up preconditioner
	itl::pc::ilu_0 < mtl::compressed2D<float> > P(matrix);
	// Iterator condition
	itl::basic_iteration <float> iter(data_vector, 200, 1e-6);
	// Matrix solver
	itl::bicgstab(matrix, output, data_vector, P, iter);

	repack_vector(output, dimension);
	/*
	if (NRows <= 10 && NCols <= 10)
	{
	cout << "Output: " << endl;
	for (int i=0; i<size; ++i)
		cout << output[i] << endl;
	cout << "New data: " << endl;
	for (int i=0; i<NRows; ++i)
	{
		for (int j=0; j<NCols; ++j)
		{
			cout << RasterData[i][j] << " ";
		}
		cout << endl;
	}
	}
	*/

		max_diff = 0;
		for (int i=0; i<NRows; ++i)
		{
			for (int j=0; j<NCols; ++j)
			{
				if (abs(RasterData[i][j] - last_iteration[i][j]) > max_diff)
					max_diff = RasterData[i][j] - last_iteration[i][j];
			}
		}
	if (not quiet && name == "debug" && size <100)
	{
		cout << "Output: " << endl;
		for (int i=0; i<size; ++i)
			cout << output[i] << endl;
		for (int i=0; i<NRows; ++i)
		{
			for (int j=0; j<NCols; ++j)
			{
				cout << RasterData[i][j] << " ";
			}
			cout << endl;
		}
	}
	++iter;
	} while (max_diff > epsilon && iter < max_iter);

}


//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
void LSDRasterModel::fluvial_incision( void )
{
	Array2D<float> zeta=RasterData.copy();

	// Step one, create donor "stack" etc. via FlowInfo
	LSDRaster temp(NRows, NCols, XMinimum, YMinimum, DataResolution, NoDataValue, zeta);
	LSDFlowInfo flow(boundary_conditions, temp);
	vector <int> nodeList = flow.get_SVector();
	int numNodes = nodeList.size();
	int node, row, col, receiver, receiver_row, receiver_col;
	float drainageArea, dx, streamPowerFactor;
	float root_2 = pow(2, 0.5);
	float K = get_K();

	if (not quiet && name == "debug" && NRows <= 10 && NCols <= 10)
	{
		cout << "Drainage area: " << endl;
		for (int i=0; i<NRows*NCols; ++i)
		{
			drainageArea = flow.retrieve_contributing_pixels_of_node(i) *  DataResolution * DataResolution;	
			cout << drainageArea << " ";
			if (((i+1)%NCols) == 0)
				cout << endl;
		}
	}

	// Step two calculate new height
	//for (int i=numNodes-1; i>=0; --i)
	for (int i=0; i<numNodes; ++i)
	{
		node = nodeList[i];
		flow.retrieve_current_row_and_col(node, row, col);
		flow.retrieve_receiver_information(node, receiver, receiver_row, receiver_col);
		drainageArea = flow.retrieve_contributing_pixels_of_node(node) *  DataResolution * DataResolution;	
		if (not quiet && name == "debug" && NRows <= 10 && NCols <= 10)
		{
			cout << row << ", " << col << ", " << receiver_row << ", " << receiver_col << endl;
			cout << flow.retrieve_flow_length_code_of_node(node) << endl;
			cout << drainageArea << endl;
		}
		switch (flow.retrieve_flow_length_code_of_node(node))
		{
			case 0:
				dx = -99;
				break;
			case 1:
				dx = DataResolution;
				break;
			case 2:
				dx = DataResolution * root_2;
				break;
			default:
				dx = -99;
				break;
		}
		if (abs(n - 1) < 0.0001)
		{
			if (dx == -99)
				continue;
			if (node != receiver)
			{
			streamPowerFactor = K * pow(drainageArea, m) * (timeStep / dx);
			zeta[row][col] = (zeta[row][col] + zeta[receiver_row][receiver_col] * streamPowerFactor) /
						 (1 + streamPowerFactor);
			}
		}
		else
		{
			if (dx == -99)
				continue;
			float new_zeta = zeta[row][col];
			float old_zeta = zeta[row][col];

			float epsilon;
			float streamPowerFactor = K * pow(drainageArea, m) * timeStep;
			float slope;
			do
			{
				slope = (new_zeta - zeta[receiver_row][receiver_col]) / dx;
				epsilon = (new_zeta - old_zeta + streamPowerFactor * pow(slope, n)) /
						 (1 + streamPowerFactor * (n/dx) * pow(slope, n-1));
				new_zeta -= epsilon;
			} while (abs(epsilon > 0.001));
		}
	}
	//return LSDRasterModel(NRows, NCols, XMinimum, YMinimum, DataResolution, NoDataValue, zeta);
	this->RasterData = zeta;
}

void LSDRasterModel::wash_out( void )
{
	if (threshold_drainage < 0 || not hillslope || not fluvial)
		return;
	Array2D<float> zeta=zeta_old.copy();
	int node;
	float DrainageArea;

	// Step one, create donor "stack" etc. via FlowInfo
	LSDRaster temp(NRows, NCols, XMinimum, YMinimum, DataResolution, NoDataValue, zeta);
	LSDFlowInfo flow(boundary_conditions, temp);
	
	for (int i=0; i<NRows; ++i)
	{
		for (int j=0; j<NCols; ++j)
		{
			node = flow.retrieve_node_from_row_and_column(i, j);
			DrainageArea = flow.retrieve_contributing_pixels_of_node(node) *  DataResolution * DataResolution;	
			if (DrainageArea > threshold_drainage)
				RasterData[i][j] = zeta_old[i][j];
		}
	}
}

LSDRaster LSDRasterModel::fluvial_erosion_rate(float timestep, float K, float m, float n, vector <string> boundary)
{
	Array2D<float> erosionRate(NRows, NCols, NoDataValue);
	Array2D<float> zeta = RasterData.copy();

	// Step one, create donor "stack" etc. via FlowInfo
	LSDFlowInfo flow(boundary, *this);
	vector <int> nodeList = flow.get_SVector();
	int numNodes = nodeList.size();
	int node, row, col, receiver, receiver_row, receiver_col;
	float drainageArea, dx, streamPowerFactor;
	float root_2 = pow(2, 0.5);

	// Step two calculate new height
	for (int i=numNodes-1; i>=0; --i)
	{
		node = nodeList[i];
		flow.retrieve_current_row_and_col(node, row, col);
		flow.retrieve_receiver_information(node, receiver, receiver_row, receiver_col);
		drainageArea = flow.retrieve_contributing_pixels_of_node(node) *  DataResolution * DataResolution;	
		switch (flow.retrieve_flow_length_code_of_node(node))
		{
			case 0:
				dx = -99;
				break;
			case 1:
				dx = DataResolution;
				break;
			case 2:
				dx = DataResolution * root_2;
				break;
			default:
				dx = -99;
				break;
		}
		if (abs(n - 1) < 0.0001)
		{
			if (node == receiver)
			{
				erosionRate[row][col] = 0;
			}
			else
			{
			streamPowerFactor = K * pow(drainageArea, m) * (timestep / dx);
			erosionRate[row][col] = ((RasterData[row][col] + RasterData[receiver_row][receiver_col] * streamPowerFactor) /
						 (1 + streamPowerFactor) - RasterData[row][col]) / timestep;
			}
		}
		else
		{
			float new_zeta = RasterData[row][col];
			float old_zeta = RasterData[row][col];

			float epsilon;
			float streamPowerFactor = K * pow(drainageArea, m) * timestep;
			float slope;
			do
			{
				slope = (new_zeta - zeta[receiver_row][receiver_col]) / dx;
				epsilon = (new_zeta - old_zeta + streamPowerFactor * pow(slope, n)) /
						 (1 + streamPowerFactor * (n/dx) * pow(slope, n-1));
				new_zeta -= epsilon;
			} while (abs(epsilon > 0.001));
			erosionRate[row][col] = (new_zeta - old_zeta) / timestep;
		}
	}
	return LSDRaster(NRows, NCols, XMinimum, YMinimum, DataResolution, NoDataValue, erosionRate);
}

/*
LSDRasterModel LSDRasterModel::run_isostatic_correction( void )
{
	// Wrapper method for fortran program to calculate isostatic correction for
	// A given load on a plate
	// Original C program written by Jon Pelletier
	// Edited by James Jenkinson
	
	Array2D <float> output(NRows, NCols, NoDataValue);

	// Run Routine (Pelletier)
	flex2d(NRows, NCols, DataResolution, NoDataValue, RasterData, output);

	return LSDRasterModel(NRows, NCols, XMinimum, YMinimum, DataResolution, NoDataValue, output);
}
*/

void LSDRasterModel::random_surface_noise( float min, float max )
{
	// Seed random numbers
	short dimension;
	int size;
	bool periodic;
	interpret_boundary(dimension, periodic, size);
	int start_i, end_i;
	int start_j, end_j;

	if (dimension == 0)
	{
		start_i = 1; end_i = NRows-2;
		start_j = 0; end_j = NCols-1;
	}
	else
	{
		start_i = 0; end_i = NRows-1;
		start_j = 1; end_j = NCols-2;
	}

	srand( static_cast <unsigned> (time(0)) );

	// Add random float to each pixel
	for (int i=start_i; i<=end_i; ++i)
	{
		for (int j=start_j; j<=end_j; ++j)
		{
			if (is_base_level(i, j))
				continue;
			RasterData[i][j] += static_cast <float> ( rand()) / static_cast <float> (RAND_MAX/(max-min)) + min;
		}
	}
}

Array2D <float> LSDRasterModel::generate_uplift_field( int mode, float max_uplift )
{
	// Generates an uplift field from some default functions
	// Maybe worth splitting into different methods?
	Array2D <float> uplift(NRows, NCols);

	for (int i=0; i<NRows; ++i)
	{
		for (int j=0; j<NCols; ++j)
		{
			uplift[i][j] = get_uplift_at_cell(i, j);
		}
	}
	return uplift;
}

float LSDRasterModel::get_uplift_at_cell(int i, int j)
{
	float result;

	// Gaussian parameters
	int mu_i = NRows/2;
	int mu_j = NCols/2;
	float sigma_i = NRows/10;
	float sigma_j = NCols/10;

	if (is_base_level(i,j))
		return 0;		
	else
		switch (uplift_mode)
		{
			case 1:		// Tilt block
				result = (NRows - i - 1) * get_max_uplift() / ((float) NRows - 1);
				break;
			case 2:		// Gausian
				
				result = get_max_uplift()*pow(1.1, -((i-mu_i)*(i-mu_i)/(2*sigma_i*sigma_i) + (j-mu_j)*(j-mu_j)/(2*sigma_j*sigma_j) ));
				break;
			case 3:
				result = get_max_uplift() * ( -pow((2.0*i/(NRows-1) - 1),2) - pow((2.0*j/(NCols-1) - 1), 2) + 1);
				if (result < 0)
					result = 0;
				break;
				
			default:
				result = get_max_uplift();
				break;
		}
	return result * timeStep;
}

void LSDRasterModel::Airy_isostasy( void )
{
	// Density of materials (kg.m^{-3})
	float rho_c = 2650;
	float rho_m = 3300;

	float load;		// total load at each cell (elevation + root)
	float zeta_root;	// Height per depth of root
	zeta_root = (rho_m - rho_c) / rho_c;

	for (int i=0; i<NRows; ++i)
	{
		for (int j = 0; j<NCols; ++j)
		{
			load = RasterData[i][j] + root_depth[i][j];
			root_depth[i][j] = load / (1 + zeta_root);
			RasterData[i][j] = load - root_depth[i][j];
		}
	}
}

void LSDRasterModel::flexural_isostasy( float alpha )
{
	int iter=0, max_iter = 200;
	Array2D<float> old_root;
	Array2D<float> difference;
	float max_error;
	float epsilon=0.0001;
	stringstream ss;

	do {
		++iter;
		max_error = 0;
		old_root = root_depth.copy();
		root_depth = calculate_root();
		difference = root_depth - old_root;

		if (not quiet && name == "debug" && NRows <= 10 && NCols<= 10)
		{
			cout << "Topography: " << endl;
		for (int i=0; i<NRows; ++i)
		{
			for (int j=0; j<NCols; ++j)
			{
				cout << RasterData[i][j] << " ";
			}
			cout << endl;
		}
			cout << "Root: " << endl;
		for (int i=0; i<NRows; ++i)
		{
			for (int j=0; j<NCols; ++j)
			{
				cout << root_depth[i][j] << " ";
			}
			cout << endl;
		}
		cout << endl;
		cout << "Difference: " << endl;
		for (int i=0; i<NRows; ++i)
		{
			for (int j=0; j<NCols; ++j)
			{
				cout << difference[i][j] << " ";
			}
			cout << endl;
		}
		}

		//RasterData += difference;
		for (int i = 0; i < NRows; ++i)
		{
			for (int j=0; j<NCols; ++j)
			{
				RasterData[i][j] -= (difference[i][j] * alpha);
				root_depth[i][j] = old_root[i][j] + (difference[i][j] * alpha);
				if (abs(difference[i][j]) > max_error)
					max_error = abs(difference[i][j]);
			}
		}
		//cout << max_error << endl;

		//if (name == "debug")
		//{
			ss.str("");
			ss << "step" << iter;
			write_root(ss.str(), "asc");
			ss.str("");
			ss << "step_raster" << iter;
			write_raster(ss.str(), "asc");
		//}
		
	} while (max_error > epsilon && iter < max_iter);
}

void LSDRasterModel::flexural_isostasy_alt( void )
{
	Array2D<float> old_root;
	Array2D<float> difference;

	old_root = root_depth.copy();
	root_depth = calculate_root();
	difference = root_depth - old_root;

	if (not quiet && name == "debug" && NRows <= 10 && NCols<= 10)
	{
		cout << "Topography: " << endl;
	for (int i=0; i<NRows; ++i)
	{
		for (int j=0; j<NCols; ++j)
		{
			cout << RasterData[i][j] << " ";
		}
		cout << endl;
	}
		cout << "Root: " << endl;
	for (int i=0; i<NRows; ++i)
	{
		for (int j=0; j<NCols; ++j)
		{
			cout << root_depth[i][j] << " ";
		}
		cout << endl;
	}
	cout << endl;
	cout << "Difference: " << endl;
	for (int i=0; i<NRows; ++i)
	{
		for (int j=0; j<NCols; ++j)
		{
			cout << difference[i][j] << " ";
		}
		cout << endl;
	}
	}

	//RasterData += difference;
	for (int i = 0; i < NRows; ++i)
	{
		for (int j=0; j<NCols; ++j)
		{
			RasterData[i][j] -= (difference[i][j]);
			root_depth[i][j] = old_root[i][j] + (difference[i][j]);
		}
	}
}

Array2D <float> LSDRasterModel::calculate_root( void )
{
	// Calculate padding size
	int Ly = pow(2, ceil(log(NRows)/log(2)));
	int Lx = pow(2, ceil(log(NCols)/log(2)));

	// Array structures for fourier coefficients
	Array2D <float> real_coeffs(Ly, Lx);		// Real part of fourier coefficients	
	Array2D <float> imag_coeffs(Ly, Lx);		// Imaginary part "                "
	Array2D <float> detrend(Ly, Lx, 0.0);	// Detrended raster data
	Array2D <float> trend(NRows, NCols);		// Trend plane
	Array2D <float> output(NRows, NCols, 0.0);	// Output Array

	// Detrend the data
	detrend2D(RasterData, detrend, trend);
	
	// Set up parameters
	/*
	float E = 10E9;		// young's modulus (kg.m^{-1}.s^{-2} || Pa
	float Te = 10E3;		// Elastic thickness (m)
	float v = 0.20;		// poisson ratio
	*/
	//float D = E * pow(Te, 3) / (12 * (1 - v*v));
	float D = rigidity;

	float rho_c = 2650;		// density of crust (kg.m^3)
	float rho_m = 3300;		// density of mantle (kg.m^3)
	float pi = 3.14159265359;	// pi
	float g = 9.81;		// gravitational acceleration (m.s^{-2})

	//cout << D << endl;

	// Calculate fourier transfor coefficients of load
	dfftw2D_fwd(detrend, real_coeffs, imag_coeffs, -1);

	// Shift the dataset
	Array2D <float> real_shift(Ly, Lx);
	Array2D <float> imag_shift(Ly, Lx);
	shift_spectrum(real_coeffs, imag_coeffs, real_shift, imag_shift);

	// Multiply coefficients by function (see Peletier, 2008)
	float coeff;

	//cout << "  Solving isostasy with Vening-Meinesz method" << endl;
	for (int i=0; i<Ly; ++i)
	{
		for (int j=0; j<Lx; ++j)
		{
			//coeff = rho_m/rho_c - 1 + (D / (rho_c*g)) * pow(pi*(pow(((float)i/Ly)*((float)i/Ly)
			//		 + ((float)j/Lx)*((float)j/Lx), 0.5)), 4);
			//coeff = (rho_c / (rho_m - rho_c)) / ((D * pow(pi*(pow(((float)i/Ly)*((float)i/Ly)
			//		 + ((float)j/Lx)*((float)j/Lx), 0.5)), 4))/((rho_m-rho_c)*g) + 1);
			coeff = (rho_c/ (rho_m-rho_c)) / (1 + 4*(4*D/(pow((rho_m-rho_c)*g,0.5) * pow(pi*(pow(((float)i/Ly)*((float)i/Ly)
					 + ((float)j/Lx)*((float)j/Lx), 0.5)), 4))));
			real_shift[i][j] *= coeff;
			imag_shift[i][j] *= coeff;
		}
	}

	// De-shift spectrum
	shift_spectrum_inv(real_shift, imag_shift, real_coeffs, imag_coeffs);

	// Reconstruct array from new fourier coefficients
	dfftw2D_inv(real_coeffs, imag_coeffs, detrend, 1);

	// Reapply trend plane
	for (int i=0; i<NRows; ++i)
	{
		for (int j=0; j<NCols; ++j)
		{
			if (i==0 && boundary_conditions[0][0] == 'b')
				output[i][j] = 0;
			else if (j==0 && boundary_conditions[3][0] == 'b')
				output[i][j] = 0;
			else if (i==NRows-1 && boundary_conditions[2][0] == 'b')
				output[i][j] = 0;
			else if (j==NCols-1 && boundary_conditions[1][0] == 'b')
				output[i][j] = 0;
			else
				output[i][j] = detrend[i][j]/(Lx*Ly) + trend[i][j];
		}
	}
	
	// DEBUG: print out isostasy map
	//LSDRaster temp(NRows, NCols, XMinimum, YMinimum, DataResolution, NoDataValue, output);
	//temp.write_raster("output", "asc");
	//root_depth = output;
	return output;
}

Array2D<float> LSDRasterModel::calculate_airy( void )
{
	float rho_c = 2650;		// density of crust (kg.m^3)
	float rho_m = 3300;		// density of mantle (kg.m^3)

	Array2D <float> airy(NRows, NCols, 0.0);

	for (int i = 0; i<NRows; ++i)
	{
		for (int j = 0; j <NCols; ++j)
		{
			airy[i][j] = RasterData[i][j] * rho_c/(rho_m-rho_c);
		}
	}
	return airy;
}

bool LSDRasterModel::is_base_level( int i, int j )
{
	if (i == 0 && boundary_conditions[0][0] == 'b')
		return true;
	else if (j == 0 && boundary_conditions[3][0] == 'b')
		return true;
	else if (i == NRows-1 && boundary_conditions[2][0] == 'b')
		return true;
	else if (j == NCols-1 && boundary_conditions[1][0] == 'b')
		return true;
	else
		return false;
}

void LSDRasterModel::print_parameters( void )
{
	if (quiet)
		return;
	cout << "\n========================================================" << endl;
	cout << "\nModel run: " << name << endl;
	cout << "\nFrom 0 to " << endTime << " years, in increments of " << timeStep << endl;
	cout << NRows << " by " << NCols << endl;
	cout << "Cells " << DataResolution << " metres wide." << endl;

	cout << "\n---------------------------------" << endl;
	cout << "Boundary conditions: " << endl;
	
	for (int i=0; i<4; ++i)
	{
		switch (i) {
			case 0:
				cout << "North:\t"; break;
			case 1:
				cout << "East:\t"; break;
			case 2:
				cout << "South:\t"; break;
			case 3:
				cout << "West:\t"; break;
		}
		if (boundary_conditions[i][0] == 'b') 		cout << "Base level" << endl;
		else if (boundary_conditions[i][0] == 'p') 	cout << "Periodic" << endl;
		else						cout << "No flow" << endl;
	}

	cout << "\n---------------------------------" << endl;
	if (fluvial)
	{
		cout << "Fluvial:\tOn" << endl;
		cout << "\nFLUVIAL PARAMETERS:" << endl;
		cout << "\tK:\t\t" << K_fluv << endl;
		cout << "\tm:\t\t" << m << endl;
		cout << "\tn:\t\t" << n << endl;
	}
	else
		cout << "\nFluvial:\tOff" << endl;

	cout << "\n---------------------------------" << endl;
	if (hillslope)
	{
		cout << "Hillslope:\tOn\t" << ((nonlinear) ? "Non-linear" : "Linear") << endl;
		cout << "\nSOIL PARAMTERS:" << endl;
		cout << "\tD:\t\t" << K_soil << endl;
		if (nonlinear)
			cout << "\tCritical slope:\t" << S_c << endl;
	}
	else
		cout << "Hillslope:\tOff" << endl;

	cout << "\n---------------------------------" << endl;
	cout << "\nIsostasy:\t" << ((isostasy) ? "On" : "Off") << endl;
	if (isostasy)
		cout << "\tModel:\t\t" << ((flexure) ? "Flexural" : "Airy") << endl;
	
	cout << "\n========================================================" << endl;
	cout << "\n" << endl;
}

void LSDRasterModel::write_report( void )
{
	static ofstream outfile;
	if (reporting && current_time > report_delay)
	{
	if (not outfile.is_open())
	{
		// Headers
		outfile.open((report_name + "_report").c_str());
		outfile << name << endl;
		outfile << "Time\t";
		outfile << "Periodicity\t";
		outfile << ((fluvial) ? "K\t" : "");
		outfile << ((hillslope) ? "D\t" : ""); 
		outfile << "Erosion\t";
		outfile << "Total erosion\t";
		outfile << "Steady\t";
		outfile << "Max_height\t";
		outfile << "Mean_height\t";
		outfile << "Relief-3px\t";
		outfile << "Relief-10m\t";
		//outfile << "Relief-30m\t";
		outfile << "Drainage-20m2\t";
		outfile << "Drainage-200m2\t";
		//outfile << "Drainage-500m2\t";
		outfile << endl;
	}
	if (not recording)
		check_recording();
	if (print_erosion_cycle)
		erosion_cycle_field = Array2D<float>(NRows, NCols, 0.0);

	outfile << current_time << "\t";
	outfile << periodicity << "\t";
	if (fluvial) 	outfile << get_K() << "\t";
	if (hillslope) 	outfile << get_D() << "\t";
	}

	// Calculate erosion across landscape
	erosion_last_step = erosion;
	erosion = 0.0;
	float e;
	int n = 0;
	for (int i=0; i<NRows; ++i)
	{
		for (int j=0; j<NCols; ++j)
		{
			e = get_erosion_at_cell(i,j);
			if (print_erosion_cycle && ((initial_steady_state || cycle_steady_check) && (K_mode !=0 || D_mode != 0)))
			{
				erosion_cycle_field[i][j] += e;
			}
			if (not is_base_level(i,j))
			{
				erosion += e;
				++n;
			}
		}
	}
	//cout << "\nn: " << n << endl;
	erosion /= n;
	// Add to total erosion count
	if (recording)
		total_erosion += erosion;
	// Calculate local response
	if (erosion > erosion_last_step)
		max_erosion = erosion;
	else if (erosion < erosion_last_step)
		min_erosion = erosion;
	if (min_erosion != -99 && max_erosion - min_erosion > response)
		response = max_erosion - min_erosion;
	if (recording)
	{
		if (erosion > max_erosion)
			max_erosion = erosion;
		if (min_erosion == -99 || erosion < min_erosion)
			min_erosion = erosion;
	}
	float mean_elev, max_elev, relief0, relief10;
	max_elev = max_elevation();
	mean_elev = mean_elevation();
	relief0 = mean_relief(0);
	relief10 = mean_relief(10);
	if (reporting && current_time > report_delay)
	{
		outfile << erosion << "\t";
		outfile << total_erosion << "\t";
		outfile << steady_state << "\t";
		outfile << max_elev << "\t";
		outfile << mean_elev << "\t";
		outfile << relief0 << "\t";
		outfile << relief10 << "\t";
		//outfile << mean_relief(30) << "\t";
		//outfile << mean_drainageDensity(500);
		outfile << endl;
	}
	if ((initial_steady_state || cycle_steady_check) && (K_mode !=0 || D_mode != 0))
		cycle_report(mean_elev, relief0, relief10);
}

void LSDRasterModel::cycle_report( float elev, float relief0, float relief10)
{
	// There's got to be a better way to design this method, I don't like it
	static ofstream outfile;
	static int phase_pos = 1;
	if ( not outfile.is_open() && reporting && current_time > report_delay)
	{
		outfile.open((report_name + "_cycle_report").c_str());		
		outfile << name << endl;
		outfile << "Cycle\t";
		outfile << "Start_time\t";
		outfile << "End_time\t";
		outfile << "Periodicity\t";
		outfile << "Erosion\t";
		outfile << "Erosion_response\t";
		outfile << "Elevation\t";
		outfile << "Elevation_response\t";
		outfile << "Relief-3px\t";
		outfile << "Relief-3px_response\t";
		outfile << "Relief-10m\t";
		outfile << "Relief-10m_response\t";
		outfile << "Drainage-20m2\t";
		outfile << "Drainage-20m2_response\t";
		outfile << "Drainage-200m2\t";
		outfile << "Drainage-200m2_response\t";
		outfile << endl;
	}
	static float mean_eros=0,  mean_elev=0,  mean_relief0=0,  mean_relief10=0;
	static float max_eros=0,   max_elev=0,   max_relief0=0,   max_relief10=0;
	static float min_eros=-99, min_elev=-99, min_relief0=-99, min_relief10=-99;
	static int n = 0;
	static float start_time = current_time;

	if (current_time == 0)
	{
			mean_eros=0,  mean_elev=0,  mean_relief0=0,  mean_relief10=0;
			max_eros=0,   max_elev=0,   max_relief0=0,   max_relief10=0;
			min_eros=-99, min_elev=-99, min_relief0=-99, min_relief10=-99;
			n= 0;
	}
	// Check next cycle number
	current_time += timeStep;
	float p = periodicity;


	if (cycle_steady_check)
	{
		if (erosion_cycle_record.size() == 0)
			erosion_cycle_record = vector<float> (5, -99);
	else
		erosion_cycle_record.empty();
	}

	if (periodic_parameter(1, 1) > 1)
	{
		if (phase_pos == 0)
		{
			++cycle_number;
			if(reporting && current_time > report_delay)
			{
				outfile << cycle_number-1 << "\t";
				outfile << start_time << "\t";
				outfile << current_time-timeStep << "\t";
				outfile << p << "\t";
				outfile << mean_eros/n << "\t";
				outfile << max_eros-min_eros << "\t";
				outfile << mean_elev/n << "\t";
				outfile << max_elev - min_elev << "\t";
				outfile << mean_relief0/n << "\t";
				outfile << max_relief0-min_relief0 << "\t";
				outfile << mean_relief10/n << "\t";
				outfile << max_relief10-min_relief10 << "\t";
				outfile << endl;
				start_time = current_time-timeStep;
			}
			if (print_erosion_cycle)
			{
				for (int i=0; i<NRows; ++i)
				{
					for (int j=0; j<NCols; ++j)
					{
						erosion_cycle_field[i][j] /= n;
					}
				}
				stringstream ss;
				ss << name << cycle_number -1 << "_cycle_erosion";
				LSDRaster * e_cycle;
				e_cycle = new LSDRaster(NRows, NCols, XMinimum, YMinimum, DataResolution, NoDataValue, erosion_cycle_field);
				e_cycle->write_raster(ss.str(), "asc");
				delete e_cycle;
				erosion_cycle_field = Array2D<float>(NRows, NCols, 0.0);
			}

			if (cycle_steady_check)
			{
				for (int i=0; i<4; ++i)
					erosion_cycle_record[i] = erosion_cycle_record[i+1];
				erosion_cycle_record[4] = mean_eros/n;
			}

			mean_eros=0,  mean_elev=0,  mean_relief0=0,  mean_relief10=0;
			max_eros=0,   max_elev=0,   max_relief0=0,   max_relief10=0;
			min_eros=-99, min_elev=-99, min_relief0=-99, min_relief10=-99;
			n = 0;
		}
		phase_pos = 1;
	}
	else
	{
		phase_pos = 0;
	}
	current_time -= timeStep;
	mean_elev += elev;
	mean_eros += erosion;
	mean_relief0 += relief0;
	mean_relief10 += relief10;

	if (elev > max_elev) max_elev = elev;
	if (erosion > max_eros) max_eros = erosion;
	if (relief0 > max_relief0) max_relief0 = relief0;
	if (relief10 > max_relief10) max_relief10 = relief10;

	if (min_elev==-99 || elev<min_elev) min_elev=elev;
	if (min_eros==-99 || erosion<min_eros) min_eros=erosion;
	if (min_relief0==-99 || relief0<min_relief0) min_relief0=relief0;
	if (min_relief10==-99 || relief10<min_relief10) min_relief10=relief10;

	++n;
}

void LSDRasterModel::final_report( void )
{
	ofstream final_report;
	final_report.open((report_name + "_final").c_str());
	final_report << name << endl;

	float run_time;
	run_time = (K_mode != 0 || D_mode != 0) ? current_time - time_delay - periodicity : current_time - time_delay;
	//cout << "Run time: " << run_time << endl;


	final_report << "Erosion\tAveraged\tResponse\tK amp\tD amp\tPeriodicity\tOvershoot" << endl;
	final_report << total_erosion << "\t" << (total_erosion / ( run_time * num_runs)) << "\t" 
		<< ((initial_steady_state) ? response/num_runs : -99) << "\t" << K_amplitude << "\t" 
		<< D_amplitude << "\t" << periodicity << "\t" << current_time - endTime << endl;
	//final_report << "Erosion\tResponse\tK amp\tD amp\tPeriodicity" << endl;
	//final_report << total_erosion << "\t" << ((initial_steady_state) ? max_erosion - min_erosion : -99) << "\t" << K_amplitude << "\t" 
}

void LSDRasterModel::print_rasters( int frame )
{
	cout << endl;
	static ofstream outfile;
	if (not outfile.is_open())
	{
		outfile.open(("."+name+"_frame_metadata").c_str());
		outfile << name << endl;
		outfile << "Frame_num\t";
		outfile << "Time\t";
		outfile << "K\t";
		outfile << "D\t";
		outfile << "Erosion\t";
		outfile << "Max_uplift\t";
		outfile << endl;
	}
	outfile << frame << "\t";
	outfile << current_time << "\t";
	outfile << get_K() << "\t";
	outfile << get_D() << "\t";
	outfile << erosion << "\t";
	outfile << get_max_uplift() << "\t";
	outfile << endl;

	stringstream ss;
	if (print_elevation)
	{
		ss << name << frame;
		this->write_raster(ss.str(), "asc");
	}
	if (print_hillshade)
	{
		ss.str("");
		ss << name << frame << "_hillshade";
		LSDRaster * hillshade;
		hillshade = new LSDRaster(*this);
		*hillshade = this->hillshade(45, 315, 1);
		hillshade->write_raster(ss.str(), "asc");
		delete hillshade;
	}
	if (print_erosion)
	{
		ss.str("");
		ss << name << frame << "_erosion";
		Array2D <float> erosion_field = calculate_erosion_rates( );
		LSDRaster * erosion;
		erosion = new LSDRaster(NRows, NCols, XMinimum, YMinimum, DataResolution, NoDataValue, erosion_field);
		erosion->write_raster(ss.str(), "asc");
		delete erosion;
	}

	if (print_slope_area)
	{
		ss.str("");
		ss << name << frame << "_sa";
		slope_area_data( name+"_sa");
	}
}

void LSDRasterModel::slope_area_data( string name )
{
	ofstream outfile;
	outfile.open((name).c_str());

	LSDRaster slope;
	Array2D<float> a, b, c, d, e, f;
	Array2D<float> drainage_array(NRows, NCols, 0.0);
	LSDFlowInfo flowData(boundary_conditions, *this);
	int node;

	calculate_polyfit_coefficient_matrices(DataResolution, a, b, c, d, e, f);
	slope = calculate_polyfit_slope(d, e);

	for (int i=0; i<NRows; ++i)
	{
		for (int j=0; j<NCols; ++j)
		{
			node = flowData.retrieve_node_from_row_and_column(i, j);
			drainage_array[i][j] = flowData.retrieve_contributing_pixels_of_node(node) * DataResolution;
		}
	}
	LSDRaster drainage(NRows, NCols, XMinimum, YMinimum, DataResolution,
		NoDataValue, drainage_array); 

	drainage.calculate_polyfit_coefficient_matrices(DataResolution, a, b, c, d, e, f);
	drainage = drainage.calculate_polyfit_elevation(f);

	// Write data
	outfile << name << endl;
	outfile << "Elevation\tSlope\tArea" << endl;

	for (int i=0; i<NRows; ++i)
	{
		for (int j=0; j<NCols; ++j)
		{
			if (slope.get_data_element(i,j) == NoDataValue || RasterData[i][j] == NoDataValue)
			{
				continue;
			}
			else
			{
				outfile << RasterData[i][j];
				outfile << "\t" << slope.get_data_element(i, j);
				outfile << "\t" << drainage.get_data_element(i, j); 
				outfile << endl;
			}
		}
	}

	outfile.close();
}

void LSDRasterModel::make_template_param_file(string filename)
{
	ofstream param;
	param.open(filename.c_str());

	param << "# Template for parameter file" << endl;
	param << "Run Name:\t\ttemplate" << endl;
	param << "NRows:\t\t\t100" << endl;
	param << "NCols:\t\t\t100" << endl;
	param << "Resolution:\t\t1" << endl;
	param << "Boundary code:\t\tbnbn\tNorth, east, south, west" << endl;
	param << "# b = base level, p = periodic, n = no flow (default)" << endl;
	param << "Time step:\t\t50" << endl;
	param << "End time:\t\t2000" << endl;
	param << "End time mode:\t\t0\t(if 1, wait for steady state to set the time to count down)" << endl;
	param << "Uplift mode:\t\t0\tBlock uplift" << endl;
	param << "Max uplift:\t\t0.001" << endl;
	param << "Tolerance:\t\t0.0001" << endl;
	param << "Print interval:\t\t5" << endl;
	param << "#Periodicity:\t\t1000" << endl;

	param << "\n#####################" << endl;
	param << "Fluvial:\t\ton" << endl;
	param << "K:\t\t\t0.01" << endl;
	param << "m:\t\t\t0.5" << endl;
	param << "n:\t\t\t1" << endl;
	param << "K mode:\t\t\t0\tconstant" << endl;
	param << "#K amplitude:\t\t0.005" << endl;

	param << "\n#####################" << endl;
	param << "Hillslope:\t\ton" << endl;
	param << "Non-linear:\t\toff" << endl;
	param << "Threshold drainage:\t-1\t(if negative, ignored)" << endl;
	param << "D:\t\t\t0.05" << endl;
	param << "S_c:\t\t\t30\tdegrees" << endl;
	param << "D mode:\t\t\t0\tConstant" << endl;
	param << "#D amplitude:\t\t0.005" << endl;

	param << "\n#####################" << endl;
	param << "Isostasy:\t\toff" << endl;
	param << "Flexure:\t\toff" << endl;
	param << "Rigidity:\t\t1000000" << endl;

	param.close();
}

void LSDRasterModel::show( void )
{
	stringstream run_cmd;
	run_cmd << "animate.run('" << name << "')";

	Py_Initialize();
	PyObject *sys_path = PySys_GetObject("path");
	PyList_Append(sys_path, PyString_FromString("."));
	PyRun_SimpleString("import animate");
	PyRun_SimpleString(run_cmd.str().c_str());
	Py_Finalize();
}

float LSDRasterModel::periodic_parameter( float base_param, float amplitude )
{
	float result;
	
	if (period_mode == 3 || period_mode == 4)
		result = p_weight * sin( (current_time-time_delay-switch_delay)*2*PI/periodicity )*amplitude + 
			 (1-p_weight) * sin( (current_time-time_delay-switch_delay)*2*PI/periodicity_2)*amplitude + base_param;
	else
		result = sin( (current_time - time_delay - switch_delay) * 2 * PI / periodicity )* amplitude + base_param;

	return result;
} 

float LSDRasterModel::square_wave_parameter( float base_param, float amplitude )
{
	int wave = (current_time - time_delay - switch_delay) /  (this->periodicity / 2);
	if (wave % 2 == 0)   wave =  1;
	else                 wave = -1;

	return base_param + (wave*amplitude);
}

float LSDRasterModel::stream_K_fluv( void )
{
	static float upr_param = K_fluv;
	static float lwr_param = K_fluv;
	static float upr_t = -99;
	static float lwr_t = 0;
	static ifstream strm;
	if (not strm.is_open())
	{
		stringstream ss;
		ss << ".K_file_" << name;
		strm.open(ss.str().c_str());
	}
	
	float temp;
	bool read = true;
	
	while (current_time >= upr_t)
	{
		if (strm >> temp)
		{
			if (upr_t == -99)
				lwr_t = time_delay;
			else
				lwr_t = upr_t;
			lwr_param = upr_param;
			upr_t = temp+time_delay;
			strm >> upr_param;
			read = true;
		}
		else
		{
			read = false;
			break;
		}

	}
	if (read)
		return (upr_param - lwr_param) * (current_time-lwr_t) / (upr_t - lwr_t) + lwr_param;
	else
		return upr_param;

}

float LSDRasterModel::stream_K_soil( void )
{
	static float upr_param = K_soil;
	static float lwr_param = K_soil;
	static float upr_t = -99;
	static float lwr_t = 0;
	static ifstream strm;
	if (not strm.is_open())
	{
		strm.open("D_file");
	}

	float temp;
	bool read;
	
	while (current_time >= upr_t)
	{
		if (strm >> temp)
		{
			if (upr_t == -99)
				lwr_t = time_delay;
			else
				lwr_t = upr_t;
			lwr_param = upr_param;
			upr_t = temp+time_delay;
			strm >> upr_param;
			read = true;
		}
		else
			read = false;

	}
	if (read)
		return (upr_param - lwr_param) * (current_time-lwr_t) / (upr_t - lwr_t) + lwr_param;
	else
		return upr_param;

}

/*
float LSDRasterModel::filestream_param(ifstream & strm, float &upr_param, float &lwr_param, float &upr_t, float &lwr_t)
{
	float temp;
	bool read;
	
	while (current_time >= upr_t)
	{
		if (strm >> temp)
		{
			if (upr_t == -99)
				lwr_t = time_delay;
			else
				lwr_t = upr_t;
			lwr_param = upr_param;
			upr_t = temp+time_delay;
			strm >> upr_param;
			read = true;
		}
		else
			read = false;

	}
	if (read)
		return (upr_param - lwr_param) * (current_time-lwr_t) / (upr_t - lwr_t) + lwr_param;
	else
		return upr_param;
}
*/

float LSDRasterModel::get_K( void )
{
	if (K_mode == 3)
	{
		stringstream ss;
		ss << "cp K_file .K_file_" << name;
		system(ss.str().c_str());
		ss.str("");
		ss.str("");
		ss << "chmod -w .K_file_" << name;
		system(ss.str().c_str());
	}
		
	switch( K_mode ) {
		case 1:			// sin wave
			return (initial_steady_state || cycle_steady_check) ? periodic_parameter( K_fluv, K_amplitude ) : K_fluv;
			break;
		case 2:			// square wave
			return (initial_steady_state) ? square_wave_parameter( K_fluv, K_amplitude ) : K_fluv;
			break;
		case 3:
			return (initial_steady_state) ? stream_K_fluv() : K_fluv;
			break;
		default:		// constant
			return K_fluv;
			break;
	}
}

float LSDRasterModel::get_D( void )
{
		
	switch( D_mode ) {
		case 1:			// sin wave
			return (initial_steady_state) ? periodic_parameter( K_soil, D_amplitude ) : K_soil;
			break;
		case 2:			// square wave
			return (initial_steady_state) ? square_wave_parameter( K_soil, D_amplitude ) : K_soil;
			break;
		case 3:
			return (initial_steady_state) ? stream_K_soil() : K_soil;
			break;
		default:		// constant
			return K_soil;
			break;
	}
}

float LSDRasterModel::get_max_uplift( void )
{
	return max_uplift;
}

float LSDRasterModel::find_max_boundary(int boundary_number)
{
	float max_val = 0;
	int i, j;
	switch (boundary_number % 2)
	{
		case 0:
			if (boundary_number == 0)
				i = 0;
			else
				i = NRows - 1;
			for (j=0; j<NCols; ++j)
			{
				if (RasterData[i][j] > max_val)
					max_val = RasterData[i][j];
			}
			break;

		case 1:
			if (boundary_number == 1)
				j = NCols - 1;
			else
				j = 0;

			for (i=0; i<NRows; ++i)
			{
				if (RasterData[i][j] > max_val)
					max_val = RasterData[i][j];
			}
			break;
	}
	return max_val;
}

////////////  DAVE'S STUFF  //////////////////////
void LSDRasterModel::DAVE_wrapper( void )
{
	float North, South;
	South = 0;
	North = find_max_boundary(0);

	int problem_dimension;
	float inv_dx_S_c_2, inv_dy_S_c_2;
	float dx_front, dy_front;
	float iteration_tolerance = 0.00001;
	Array2D <float> fluvial_temp(NRows, NCols, 0.0);

	vector <int> vec_k_value_i_j;
	vector <int> vec_k_value_ip1_j;
	vector <int> vec_k_value_im1_j;
	vector <int> vec_k_value_i_jp1;
	vector <int> vec_k_value_i_jm1;

	DAVE_initiate_assembler_matrix(problem_dimension, inv_dx_S_c_2, inv_dy_S_c_2, dx_front, dy_front,
		vec_k_value_i_j, vec_k_value_ip1_j, vec_k_value_im1_j, vec_k_value_i_jp1, vec_k_value_i_jm1);

	DAVE_nonlinear_creep_timestep(uplift_field, fluvial_temp, iteration_tolerance, problem_dimension, 
		inv_dx_S_c_2, inv_dy_S_c_2, dx_front, dy_front, vec_k_value_i_j, vec_k_value_ip1_j,
		vec_k_value_im1_j, vec_k_value_i_jp1, vec_k_value_i_jm1, South, North);


}

void LSDRasterModel::DAVE_initiate_assembler_matrix(int& problem_dimension,				     
					     float& inv_dx_S_c_squared, float& inv_dy_S_c_squared, float& dx_front_term, 
               float& dy_front_term, vector<int>& vec_k_value_i_j, vector<int>& vec_k_value_ip1_j,
					     vector<int>& vec_k_value_im1_j, vector<int>& vec_k_value_i_jp1, vector<int>& vec_k_value_i_jm1)
{
  float dx = DataResolution;
  float dy = DataResolution;
  float D_nl = get_D();
	inv_dx_S_c_squared = 1/(dx*dx*S_c*S_c);
	inv_dy_S_c_squared = 1/(dy*dy*S_c*S_c);
	dx_front_term = timeStep*D_nl/(dx*dx);
	dy_front_term = timeStep*D_nl/(dy*dy);

  problem_dimension = NRows*NCols;
  DAVE_calculate_k_values_for_assembly_matrix(NRows, NCols, vec_k_value_i_j, vec_k_value_ip1_j,
											vec_k_value_im1_j, vec_k_value_i_jp1, vec_k_value_i_jm1);

}

void LSDRasterModel::DAVE_calculate_k_values_for_assembly_matrix(int NRows, int NCols, vector<int>& k_value_i_j,
                      vector<int>& k_value_ip1_j,	vector<int>& k_value_im1_j, vector<int>& k_value_i_jp1, 
                      vector<int>& k_value_i_jm1)											
{
	int N_elements_in_k_vec = (NRows-2)*NCols;

	// initialize the vectors with empty values
	vector<int> empty_vec(N_elements_in_k_vec,0);
	k_value_i_j   = empty_vec;
	k_value_ip1_j = empty_vec;
	k_value_im1_j = empty_vec;
	k_value_i_jp1 = empty_vec;
	k_value_i_jm1 = empty_vec;

	// we loop through each node
	int counter = 0;
	for (int row = 1; row<NRows-1; row++)
	{
		for (int col = 0; col<NCols; col++)
		{
			k_value_ip1_j[counter] = NCols*(row+1)+col;
			k_value_im1_j[counter] = NCols*(row-1)+col;
			k_value_i_j[counter] = NCols*row+col;

			// logic for west periodic boundary
			if(col == 0)
			{
				k_value_i_jp1[counter] = NCols*row+col+1;
				k_value_i_jm1[counter] = NCols*row+NCols-1;
			}
			// logic for east periodic boundary
			else if(col == NCols-1)
			{
				k_value_i_jp1[counter] = NCols*row;
				k_value_i_jm1[counter] = NCols*row+col-1;

			}
			// logic for rest of matrix
			else
			{
				k_value_i_jp1[counter] = NCols*row+col+1;
				k_value_i_jm1[counter] = NCols*row+col-1;
			}

			// increment counter
			counter++;
		}
	}
}

void LSDRasterModel::DAVE_nonlinear_creep_timestep(Array2D<float>& uplift_rate, Array2D<float>& fluvial_erosion_rate,
						float iteration_tolerance, int problem_dimension,
						float inv_dx_S_c_squared, float inv_dy_S_c_squared, float dx_front_term, float dy_front_term,
						vector<int>& vec_k_value_i_j, vector<int>& vec_k_value_ip1_j, vector<int>& vec_k_value_im1_j,
						vector<int>& vec_k_value_i_jp1, vector<int>& vec_k_value_i_jm1,
					  float South_boundary_elevation, float North_boundary_elevation)
{
  float dt_old = timeStep;
  float dt_new = timeStep;
  // initialise residual for upcoming loop
  float residual = 0;
  float mean_residual = iteration_tolerance+1;   // not used at present
  float max_residual = iteration_tolerance+1;
	// Loop containing adaptive timestep
  bool continue_switch = false;
  int n_iterations = 0;
  int max_iterations = 10;	
  while (continue_switch == false)
  {
    // reset zeta_old and zeta_last_iter
	  zeta_last_iter = RasterData.copy();  
    zeta_last_timestep = RasterData.copy();
    
    while ((max_residual > iteration_tolerance) && (n_iterations <= max_iterations))
  	{
      max_residual = 0;
      mean_residual = 0;
      // crunch some numbers
      DAVE_solve_assembler_matrix(uplift_rate, fluvial_erosion_rate, problem_dimension,
                    inv_dx_S_c_squared, inv_dy_S_c_squared, dx_front_term, dy_front_term, 
  							    vec_k_value_i_j, vec_k_value_ip1_j, vec_k_value_im1_j, vec_k_value_i_jp1, vec_k_value_i_jm1,
  					     		South_boundary_elevation, North_boundary_elevation);
      // check the residuals (basically this is the average elevation change between 
  		// zeta values for this iteration and the previous iteration
  		for (int row = 0; row<NRows; row++)
  		{
  			for (int col = 0; col<NCols; col++)
  			{
  				residual = sqrt( (zeta_this_iter[row][col]-zeta_last_iter[row][col])*(zeta_this_iter[row][col]-zeta_last_iter[row][col]) );
  				mean_residual += residual;
          if (residual > max_residual)
  				{
            max_residual = residual;
          }
  			}
  		}
  		mean_residual = mean_residual/float(NRows*NCols);
  		// update zeta_last_iter
  		zeta_last_iter = zeta_this_iter.copy();
  		// update n_iterations
      ++n_iterations;
    }  // end logic for while loop
    
    // Here is where we adapt the timestep if necessary.
    // If there is only one iteration, then we can try increasing the timestep
    // to reduce computation time
    if (n_iterations == 1)
    {
      dt_new = timeStep*2;
      continue_switch = true;
      cout << endl << "SPEEDING UP! New timestep is: " << dt_new << " old timestep = " << timeStep << endl;
      timeStep = dt_new;
      // initiate new mtl assembler matrix with new dt
      DAVE_initiate_assembler_matrix(problem_dimension, inv_dx_S_c_squared, inv_dy_S_c_squared, dx_front_term, dy_front_term, 
               vec_k_value_i_j, vec_k_value_ip1_j, vec_k_value_im1_j, vec_k_value_i_jp1, vec_k_value_i_jm1);
    }
    // if there are more than max_iterations, the scheme is probably not stable,
    // so decrease timestep to maintain stability. Reset the zeta rasters.
    else if (n_iterations >= max_iterations)
    {
      dt_new = timeStep/10;
      // reset zeta_last_iter
      zeta_last_iter = RasterData.copy();
      timeStep = dt_new;
      // initiate new mtl assembler matrix with new dt
      DAVE_initiate_assembler_matrix(problem_dimension, inv_dx_S_c_squared, inv_dy_S_c_squared, dx_front_term, dy_front_term, 
               vec_k_value_i_j, vec_k_value_ip1_j, vec_k_value_im1_j, vec_k_value_i_jp1, vec_k_value_i_jm1);
      
      continue_switch = false;
      cout << endl << "SLOWING DOWN! Max residual is: " << max_residual << " Iteration tolerance = " << iteration_tolerance << " New timestep is: " << dt_new << endl;
    }
    // timestep about right so that iteration converges quickly.  Carry on!
    else
    {
//       cout << "Timestep is just right! Max residual is: " << max_residual << " Iteration tolerance = " << iteration_tolerance << " timestep is: " << dt << endl;
      continue_switch = true;
    }
    // reset number of iterations
    n_iterations = 0;
  }  // end logic for while loop
  RasterData = zeta_this_iter.copy(); 
  timeStep = dt_old;
}

void LSDRasterModel::DAVE_solve_assembler_matrix(Array2D<float>& uplift_rate, Array2D<float>& fluvial_erosion_rate,
						 int problem_dimension, float inv_dx_S_c_squared, float inv_dy_S_c_squared,
					   float dx_front_term, float dy_front_term,
             vector<int>& vec_k_value_i_j, vector<int>& vec_k_value_ip1_j, vector<int>& vec_k_value_im1_j,
             vector<int>& vec_k_value_i_jp1, std::vector<int>& vec_k_value_i_jm1,
             float South_boundary_elevation, float North_boundary_elevation)
{
  // reset the zeta array for this iteration
	Array2D<float> empty_zeta(NRows,NCols,0.0);
	zeta_this_iter = empty_zeta.copy();
	// create a mtl matrix
	// NOTE: you could probably save time by creating the mtl matrix and vector
	// in main()
	mtl::compressed2D<float> mtl_Assembly_matrix(problem_dimension, problem_dimension);
	mtl::dense_vector<float> mtl_b_vector(problem_dimension,0.0);

	// assemble the matrix
  DAVE_assemble_matrix(uplift_rate, fluvial_erosion_rate, mtl_Assembly_matrix, mtl_b_vector,
						problem_dimension, inv_dx_S_c_squared, inv_dy_S_c_squared,
						dx_front_term, dy_front_term, South_boundary_elevation, North_boundary_elevation,
						vec_k_value_i_j, vec_k_value_ip1_j, vec_k_value_im1_j, vec_k_value_i_jp1, vec_k_value_i_jm1);
	// now solve the mtl system
	// Create an ILU(0) preconditioner
	itl::pc::ilu_0< mtl::compressed2D<float> > P(mtl_Assembly_matrix);
	mtl::dense_vector<float> mtl_zeta_solved_vector(problem_dimension);
	itl::basic_iteration<float> iter(mtl_b_vector, 500, 1.e-8);
	bicgstab(mtl_Assembly_matrix, mtl_zeta_solved_vector, mtl_b_vector, P, iter);
  
	// now reconstitute zeta
	int counter = 0;//NCols;
	for (int row = 0; row<NRows; row++)
	{
		for (int col = 0; col < NCols; col++)
		{
			zeta_this_iter[row][col] = mtl_zeta_solved_vector[counter];
			counter++;
		}
	}
}

void LSDRasterModel::DAVE_assemble_matrix(Array2D<float>& uplift_rate, Array2D<float>& fluvial_erosion_rate,
             mtl::compressed2D<float>& mtl_Assembly_matrix, mtl::dense_vector<float>& mtl_b_vector,
						 int problem_dimension, float inv_dx_S_c_squared, float inv_dy_S_c_squared, 
					   float dx_front_term, float dy_front_term,
             float South_boundary_elevation, float North_boundary_elevation,
             vector<int>& vec_k_value_i_j, vector<int>& vec_k_value_ip1_j,vector<int>& vec_k_value_im1_j,
						 vector<int>& vec_k_value_i_jp1, vector<int>& vec_k_value_i_jm1)
{
	// the coefficients in the assembly matrix
	float A,B,C,D;

	// reset the assembly and b vector
	mtl_Assembly_matrix = 0.0;
	mtl_b_vector = 0.0;

	// create the inserter. This is deleted when this function is exited
	mtl::matrix::inserter< mtl::compressed2D<float> > ins(mtl_Assembly_matrix);

	// first we assemble the boundary nodes. First the nodes in row 0 (the south boundary)
	for (int k = 0; k<NCols; k++)
	{
		ins[k][k] << 1.0;
		mtl_b_vector[k] = South_boundary_elevation;
	}

	// now assemble the north boundary
	int starting_north_boundary = (NRows-1)*(NCols);
	int one_past_last_north_boundary = NRows*NCols;
	for (int k = starting_north_boundary; k < one_past_last_north_boundary; k++)
	{
		ins[k][k] << 1.0;
		mtl_b_vector[k] = North_boundary_elevation;
	}
  
	// create the zeta matrix that includes the boundary conditions
	Array2D<float> zeta_for_implicit(NRows, NCols,0.0);
	for (int col = 0; col<NCols; col++)
	{
		zeta_for_implicit[0][col] = North_boundary_elevation;
		zeta_for_implicit[NRows-1][col] = South_boundary_elevation;
	}
	for (int row = 1; row<NRows-1; row++)
	{
		for (int col = 0; col<NCols; col++)
		{
			zeta_for_implicit[row][col] = zeta_last_iter[row][col];
		}
	}

	// now assemble the rest
	// we loop through each node
	int counter = 0;
	float b_value;
	int k_value_i_j,k_value_ip1_j,k_value_im1_j,k_value_i_jp1,k_value_i_jm1;
	for (int row = 1; row<NRows-1; row++)
	{
		for (int col = 0; col<NCols; col++)
		{
			k_value_ip1_j = vec_k_value_ip1_j[counter];
			k_value_im1_j = vec_k_value_im1_j[counter];
			k_value_i_j   = vec_k_value_i_j[counter];
			k_value_i_jp1 = vec_k_value_i_jp1[counter];
			k_value_i_jm1 = vec_k_value_i_jm1[counter];

      A = dy_front_term/(1 -
		        (zeta_for_implicit[row+1][col]-zeta_for_implicit[row][col])*
		        (zeta_for_implicit[row+1][col]-zeta_for_implicit[row][col])*
				inv_dy_S_c_squared);
		  B = dy_front_term/(1 -
		        (zeta_for_implicit[row][col]-zeta_for_implicit[row-1][col])*
		        (zeta_for_implicit[row][col]-zeta_for_implicit[row-1][col])*
				inv_dy_S_c_squared);
			
      b_value = zeta_last_timestep[row][col]+timeStep*uplift_rate[row][col]-timeStep*fluvial_erosion_rate[row][col];

			// logic for west periodic boundary
      if(col == 0)
			{
				C = dx_front_term/(1 -
			           (zeta_for_implicit[row][col+1]-zeta_for_implicit[row][col])*
			           (zeta_for_implicit[row][col+1]-zeta_for_implicit[row][col])*
					   inv_dx_S_c_squared);
				D = dx_front_term/(1 -
			           (zeta_for_implicit[row][col]-zeta_for_implicit[row][NCols-1])*
			           (zeta_for_implicit[row][col]-zeta_for_implicit[row][NCols-1])*
					   inv_dx_S_c_squared);

			}
			// logic for east periodic boundary
			else if(col == NCols-1)
			{
        C = dx_front_term/(1 -
			           (zeta_for_implicit[row][0]-zeta_for_implicit[row][col])*
			           (zeta_for_implicit[row][0]-zeta_for_implicit[row][col])*
					   inv_dx_S_c_squared);
				D = dx_front_term/(1 -
			           (zeta_for_implicit[row][col]-zeta_for_implicit[row][col-1])*
			           (zeta_for_implicit[row][col]-zeta_for_implicit[row][col-1])*
					   inv_dx_S_c_squared);

			}
			// logic for rest of matrix
			else
			{
				C = dx_front_term/(1 -
			           (zeta_for_implicit[row][col+1]-zeta_for_implicit[row][col])*
			           (zeta_for_implicit[row][col+1]-zeta_for_implicit[row][col])*
					   inv_dx_S_c_squared);

				D = dx_front_term/(1 -
			           (zeta_for_implicit[row][col]-zeta_for_implicit[row][col-1])*
			           (zeta_for_implicit[row][col]-zeta_for_implicit[row][col-1])*
					   inv_dx_S_c_squared);

			}
			// place the values in the assembly matrix and the b vector
      mtl_b_vector[k_value_i_j] = b_value;
			ins[k_value_i_j][k_value_ip1_j] << -A;
			ins[k_value_i_j][k_value_im1_j] << -B;
			ins[k_value_i_j][k_value_i_jp1] << -C;
			ins[k_value_i_j][k_value_i_jm1] << -D;
			ins[k_value_i_j][k_value_i_j] << 1+A+B+C+D;

			counter++;
		}
	}
}

void LSDRasterModel::write_root( string name, string ext )
{
	LSDRaster root(NRows, NCols, XMinimum, YMinimum, DataResolution, NoDataValue, root_depth);
	root.write_raster(name, ext);
}

void LSDRasterModel::snap_periodicity( void )
{
	periodicity = ceil(periodicity/timeStep) * timeStep;
}

#endif



// nonlinear_diffusion - MIGHT STILL BE A WORK IN PROGRESS - CHECK WITH SIMON
// calculate fluxes using the nonlinear soil flux model - use the buffered
// topographic raster
//------------------------------------------------------------------------------
// LSDRasterModel LSDRasterModel::nonlinear_diffusion(Array2D<float>& SlopesBetweenRows,
//                 Array2D<float>& SlopesBetweenColumns, float K_nl, float S_c, float dt)
// {
// 	// search around each node
// 	//		2
// 	//		|
// 	//  3 - N - 1
// 	//		|
// 	//		4
// 	float Q_1, Q_2, Q_3, Q_4;
// 	float inverse_S_c2 = 1/(S_c*S_c);
// 	float crit_term;
// 	float dxKnl = DataResolution*K_nl;
// 	float dyKnl = DataResolution*K_nl;
// 	float dt_over_dxdy = dt/(DataResolution*DataResolution);
//   Array2D<float> zeta = RasterData.copy();
// //	ofstream a_out;
// //	a_out.open("outs.txt");
// 
// 	for (int row = 0; row<NRows; row++)
// 	{
// 		for (int col = 0; col<NCols; col++)
// 		{
// 			// direction 1
// 			crit_term = 1 - (slopes_between_columns[row][col+1]*slopes_between_columns[row][col+1]*inverse_S_c2);
// 			if(crit_term < 0)
// 			{
// 				crit_term = 0.01;
// 			}
// 			Q_1 =  dyKnl*slopes_between_columns[row][col+1]/crit_term;
// 			//a_out << endl << endl << "col: " << col << " row: " << row << endl;
// 			//a_out << "crit term 1: " << crit_term << " Q_1: " << Q_1 << endl;
// 			// direction 2
// 			crit_term = 1 - (slopes_between_rows[row+1][col]*slopes_between_rows[row+1][col]*inverse_S_c2);
// 			if(crit_term < 0)
// 			{
// 				crit_term = 0.01;
// 			}
// 			Q_2 =  dxKnl*slopes_between_rows[row+1][col]/crit_term;
// 			//a_out << "crit term 2: " << crit_term << " Q_2: " << Q_2 << endl;
// 			// direction 3
// 			crit_term = 1 - (slopes_between_columns[row][col]*slopes_between_columns[row][col]*inverse_S_c2);
// 			if(crit_term < 0)
// 			{
// 				crit_term = 0.01;
// 			}
// 			Q_3 =  - dyKnl*slopes_between_columns[row][col]/crit_term;
// 			//a_out << "crit term 3: " << crit_term << " Q_3: " << Q_3 << endl;
// 
// 			// direction 3
// 			crit_term = 1 - (slopes_between_rows[row][col]*slopes_between_rows[row][col]*inverse_S_c2);
// 			if(crit_term < 0)
// 			{
// 				crit_term = 0.01;
// 			}
// 			Q_4 =  - dxKnl*slopes_between_rows[row][col]/crit_term;
// 			//a_out << "crit term 4: " << crit_term << " Q_4: " << Q_4 << endl;
// 			      
//       zeta[row][col] = zeta[row][col] + dt_over_dxdy*(Q_1+Q_2+Q_3+Q_4);
// 			//a_out << "dz = " << dt*(Q_1+Q_2+Q_3+Q_4)/(dx*dy) << endl;
// 		}
// 	}
// 	//a_out.close();
// 	LSDRasterModel Zeta(NRows, NCols, XMinimum, YMinimum, DataResolution, NoDataValue, zeta);
//   return Zeta;
// }

// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// EXPLICIT MODEL COMPONENTS
// ------------------------------------------------------------------------------
// A series of functions that carry out parts of the number crunching in the
// explicit version of the model
// ------------------------------------------------------------------------------
// creep_and_fluvial_timestep
// For a given timestep in the model, calculates the hillslope and channel
// sediment fluxes and adjusts topography accordingly
// ------------------------------------------------------------------------------
// LSDRasterModel LSDRasterModel::creep_and_fluvial_timestep(float& t_ime, float dt, float uplift_rate,
// 								float South_boundary_elevation, float North_boundary_elevation,
// 								float D_nl, float S_c, float k_w, float b, float K, float n, float m, 
//                 float erosion_threshold, LSDRasterModel& ZetaOld, LSDRasterModel& ZetaRasterBuff,
// 								Array2D<float>& SlopesBetweenRows, Array2D<float>& SlopesBetweenColumns,
// 								Array2D<float>& ErosionRateArray, Array2D<float>& precip_flux, Array2D<float>& Q_w, 
//                 Array2D<float>& ChannelWidthArray, Array2D<float>& TopoDivergence,
//                 Array2D<float>& FluvialErosionRateArray)
// {
// 	// Load object data into new array
//   Array2D<float> zeta = RasterData.copy();
//   
//   int any_pits = 1;
// 	t_ime+=dt;			// increment time
// 	// reset zeta_old
// 	LSDRasterModel ZetaOld(NRows, NCols, XMinimum, YMinimum, DataResolution, NoDataValue, zeta);
//   // uplift the landsape
// 	LSDRasterModel ZetaRaster = uplift_surface(UpliftRate, dt);
// 	// buffer the landscape
// 	LSDRasterModel ZetaRasterBuff = ZetaRaster.create_buffered_surf(South_boundary_elevation, North_boundary_elevation);
// 	// get slopes for creep
// 	ZetaRasterBuff.get_slopes(Array2D<float>& SlopesBetweenRows, Array2D<float>& SlopesBetweenColumns);
//   // diffuse surface with nonlinear creep
// 	nonlinear_diffusion(SlopesBetweenRows, SlopesBetweenColumns, K_nl, S_c, dt);
// 	// now update the original surface
// 	for (int row = 0; row < NCols; row++)
// 	{
// 		for (int col = 0; col < NRols; col++)
// 		{
// 			ZetaRaster.get_data_element(row,col) = ZetaRasterBuff.get_data_element(row+1,col+1);
//   	}
// 	}
//   ZetaRaster = ZetaRaster.fill(0.00001);
// 
// THIS NEEDS TO BE REPLACED BY FASTSCAPE!!!
// 	while (any_pits == 1)
// 	{
// 		// now vectorize surface to look at fluvial erosion
// 		vectorize_surface(zeta, N_rows, N_cols, vector_zeta,
// 					  		row_vec, col_vec);
// 		// now sort the surface
// 		matlab_float_sort(vector_zeta,sorted_surf_vec,sorting_index);
// 		// now route the flow
// 		any_pits = flow_routing_2Direction(sorted_surf_vec, sorting_index,
// 							row_vec, col_vec, zeta_buff, Q_w, precip_flux, N_rows, N_cols);
// 
// 		if (any_pits == 1)
// 		{
// 			//cout << "FILLING LINE 271" << endl;
// 
// 			// now fill the pits and flats of this buffered surface
// 			fill_pits_and_flats_parent(zeta_buff, N_rows+2, N_cols+2);
// 
// 			// now update the original surface with the filled surface
// 			for (int row = 0; row < N_rows; row++)
// 			{
// 				for (int col = 0; col < N_cols; col++)
// 				{
// 					zeta[row][col] = zeta_buff[row+1][col+1];
// 				}
// 			}
// 		}
// 	}
// 	
//   // now get the channel widths
// 	ChannelWidthArray = array_channel_width_wolman(Q_w,k_w,b);
// 	// calcualte the topographic divergence
// 	TopoDivergence = ZetaRaster.get_topographic_divergence();
// 	// calcualte fluvial erosion rate
// 	FluvialErosionRateArray = calculate_fluvial_erosion_rate(ChannelWidth, Q_w, TopoDivergence, K, n, m, ErosionThreshold);
// 	// erode fluvially
// 	for (int row = 0; row<NRows; row++)
// 	{
// 		for (int col = 0; col<NCols; col++)
// 		{
// 			ZetaRaster.get_data_element(row,col) -= dt*FluvialErosionRate[row][col];
// 		}
// 	}
//   // calucalte erosion rate
//   ErosionRateArray = calculate_erosion_rate_array(ZetaRaster, ZetaOld, dt);  
//   return ZetaRaster;
// }


////=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//// INITIAL TOPOGRAPHY MODULE
////=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//// There are a number of options that will be built into this to give 
//// flexibility when it comes to setting up the LEM.
//// This includes:
//// i) Artifial parabolic surfaces - other surfaces may be added later as 
//// needed
//// ii) Existing topographic datasets (for example a real DEM) - this is simply
//// done by loading the data as LSDRasterModel rather than an LSDRaster
////=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//// Initialise_parabolic_surface
//// This function generates a parabolic surface with elevations on the North
//// and South edges at zero, and in the middle at 'PeakElevation'
////----------------------------------------------------------------------------
// LSDRasterModel LSDRasterModel::create_parabolic_surface(int NRows,
//   int NCols, float DataResolution, float NoDataValue, float PeakElevation, float RandomAmplitude,
//   float EdgeOffset)
// {
//   Array2D<float> temp_surf(N_rows,N_cols,0.0);
// 	surf = temp_surf.copy();
// 
// 	long seed = time(NULL);               // seed for random number generator
// 	float local_x;
// 	float L = dx*(N_rows-1);
// 	float row_elev;
// 	float perturb;
// 
// 	for(int row = 0; row < N_rows; row++)
// 	{
// 		local_x = row*dx;
// 		row_elev = - 4.0*(local_x*local_x-local_x*L)*peak_elev / (L*L);
// 		for (int col = 0; col < N_cols; col++)
// 		{
// 			perturb = (ran3(&seed)-0.5)*random_amp;
// 			surf[row][col] = row_elev + perturb + edge_offset;
// 		}
// 	}
// 	LSDRasterModel ParabolicSurface(NRows, NCols, XMinimum, YMinimum, DataResolution, NoDataValue, surf);
//   return ParabolicSurface;
// }
////----------------------------------------------------------------------------
//// Create Nonlinear Steady State Hillslope
//// Does what it says on the tin
// LSDRasterModel LSDRasterModel::create_nonlinear_SS_hillslope(int NRows, int NCols,
//   float XMinimum, float YMinimum, float DataResolution, float NoDataValue,
//   float K_nl, float S_c, float U)
// {  
// 	float y,loc_y;
//  	float max_y = (N_rows-1)*dy;
//  	
//   float term1 = K_nl*S_c*S_c*0.5/U;
//  	float term2 = 2*U/(K_nl*S_c);
// 
// 	y = max_y;
//  	float min_zeta = term1*(  log(0.5*(sqrt(1+ (y*term2)*(y*term2)) +1))
//  	                       -sqrt(1+ (y*term2)*(y*term2))+1);
// 
// 	for (int row = 0; row<N_rows; row++)
// 	{
// 		for (int col = 0; col<N_cols; col++)
// 		{
// 			loc_y = dy*float(row);
// 			y = max_y-loc_y;
// 			zeta[row][col] = term1*(  log(0.5*(sqrt(1+ (y*term2)*(y*term2)) +1))
//  	                       -sqrt(1+ (y*term2)*(y*term2))+1) - min_zeta;
// 		}
// 	}
// 	LSDRasterModel NonLinearHillslope(NRows, NCols, XMinimum, YMinimum, DataResolution, NoDataValue, zeta);
//   return NonLinearHillslope;
// }