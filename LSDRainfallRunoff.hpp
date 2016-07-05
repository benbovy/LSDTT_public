#ifndef LSDRAINFALLRUNOFF_H
#define LSDRAINFALLRUNOFF_H


#include "TNT/tnt.h"
#include "LSDStatsTools.hpp" // This contains some spline interpolation functions already

/// @brief rainGrid is a class used to store and manipulate rainfall data.
/// @detail It can be used to interpolate or upscale rainfall data from coarser
/// resolutions/grid spacings.
/// @author DAV
class rainGrid
{
public:

  /// Default constructor -- throws an error.
  rainGrid()
  {
    create();
  }

  /// Create a raingrid from the rainfall data vector,
  /// and the raster or model domain dimensions, for the
  /// current timestep only. Specify an interpolation method
  rainGrid(std::vector< std::vector<float> >& rain_data,
           TNT::Array2D<int>& hydroindex,
           int imax, int jmax, int current_rainfall_timestep, int rf_num)
  {
    //std::cout << "Creating a LSD rainGrid object from a rainfall timeseries and hydroindex..." \
    //             << std::endl;
    create(rain_data, hydroindex, imax, jmax, current_rainfall_timestep, rf_num);
  }

  /// Create rainGrid from interpolating between sparse points and x, y coord
  /// TODO

  /// Takes a 2D array of regular gridded rainfall and interpolates
  /// it according to a Bivariate Spline. Similar to sciPy's
  /// scipy.interpolate.RectBivariateSpline
  void interpolateRainfall_RectBivariateSpline(rainGrid& raingrid);

  /// Will take a 2D array of regular gridded rainfall and interpolates
  /// it based on a TRI-variate spline. I.e. interpolates based on an
  /// extra third variable which would be terrain in most cases (see
  /// Tait et al 2006, for example)
  void interpolateRainfall_RectTrivariateSpline(rainGrid& raingrid,
                                                TNT::Array2D<double>& elevation);
  
  /// Takes the rainfall data for a current timestep and
  /// reshapes it into a 2D array. 
  void ReShapeRainfallData_2DArray();
  
  /// Upscales the 2d rainfall array to a resampled, higher resolution
  /// array, with the same grid spacing, and dimensions as the model or 
  /// raste rdomain. 
  void upscaleRainfallData();
  
  /// Writes the 2D upscaled and/or interpolated rainfall grid to 
  /// a raster output file for checking.
  void write_rainGrid_to_raster_file(double xmin, double ymin, 
                                               double cellsize,
                                               std::string RAINGRID_FNAME,
                                               std::string RAINGRID_EXTENSION);

  /// Getter for getting rainfall value
  double get_rainfall(int i, int j) const { return rainfallgrid2D[i][j]; }
 
protected:

  /// For a single instance of a 2D rainfall grid, matching the dimensions of the
  /// model domain.
  TNT::Array2D<double> rainfallgrid2D;
  /// Experimental - stores grids of rainfall data for every rainfall timestep:
  /// Warning - this could be a massive object!
  TNT::Array3D<double> rainfallgrid3D;

private:
  /// Returns an error, empty object pointless
  void create();
  /// Initialises by converting rainfall data file into grid at same
  /// grid spacing as model (or raster) domain grid spacing.
  void create(std::vector<std::vector<float> >& rain_data,
              TNT::Array2D<int>& hydroindex,
              int imax, int jmax, int current_rainfall_timestep, int rf_num);

};


/// Object for storing and calculating the saturation and hence 
/// surface runoff when using spatially
/// variable rainfall input (j_mean, but 2D, i.e. j_mean_array[i][j], same as 
/// model 

class runoffGrid
{
public:

  /// Basic constructor -- initialises arrays to domain size.
  runoffGrid(int imax, int jmax)
  {
    create(imax, jmax);
  }

  /// Create a rainfallrunoffGrid from passing params and refs to params
  runoffGrid(int current_rainfall_timestep, int imax, int jmax,
                     int rain_factor, double M,
                     const rainGrid& current_rainGrid)
  {
    /*std::cout << "Creating a LSD runoffGrid object from the" << std::endl
                 << " current rainGrid and domain parameters..." \
                 << std::endl;
                 */
    create(current_rainfall_timestep, imax, jmax,
            rain_factor, M,
            current_rainGrid);
  }  

  //void calculate_catchment_water_inputs(); // Left this is CatchmentModel object for now
  // could be moved at later date, but we need reorganisation of classes.

  /// Calculates runoff and updates the rainfallrunoffGrid object accordingly
  /// Introduced to be able to create an empty runoff object and then later initialise it,
  /// or update an exisiting runoff grid for a new timestep,
  void calculate_runoff(int rain_factor, double M, int jmax, int imax, const rainGrid &current_rainGrid);

  // Getters for runoff variables
  double get_j(int m, int n) const { return j[m][n]; }
  double get_jo(int m, int n) const { return jo[m][n]; }
  double get_j_mean(int m, int n) const { return j_mean[m][n]; }
  double get_old_j_mean(int m, int n) const { return old_j_mean[m][n]; }
  double get_new_j_mean(int m, int n) const { return new_j_mean[m][n]; }

  /// Sets the value of j_mean when calculating the hydrograh
  /// @param m, n array indices, new value to set (double)
  void set_j_mean(int m, int n, double cell_j_mean) { j_mean[m][n] = cell_j_mean; }

protected:
  TNT::Array2D<double> j, jo, j_mean, old_j_mean, new_j_mean;

private:
  void create(int imax, int jmax);
  void create(int current_rainfall_timestep, int imax, int jmax,
         int rain_factor, double M,
         const rainGrid& current_rainGrid);
};


#endif // LSDWEATHERCLIMATETOOLS_H
