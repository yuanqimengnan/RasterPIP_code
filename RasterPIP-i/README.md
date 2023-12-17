# README for RasterPIP-i

The current code is a raw version of RasterPIP-i used to solve the iPIP problem, returning a set of points within each polygon.

The code for RasterPIP-i references the RasterJoin, whose paper link is http://www.vldb.org/pvldb/vol11/p352-zacharatou.pdf. We implement RasterPIP-i by adding or deleting some codes in RasterJoin. Note that the current version may contain some codes which are not important for RasterPIP-i. They were originally designed to execute RasterJoin. We avoid such code execution by annotating and  specifying input parameters. We will provide a complete code version for RasterPIP-i only in the future.

Usage: RasterPIPi --nIter number_of_iterations --accuracy meters --joinType raster --backendIndexName path_of_points --dataset index_of_points_data --polygonList path_of_polygons --polygonDataset name_of_polygon_collection --locAttrib offset_of_the_location_attribute_in_the_record --nAttrib number_of_filtered_attributes --startTime start_time --endTime end_time --inmem --gpuMem size_in_megabytes --experiments path_of_experiment_data --selectFile path_of_result_points

--nIter: Define how many times the query should be executed.

--accuracy: The pixel length in meters.

--joinType: Setting to "raster" for RasterPIP-i.

--backendIndexName: Prefix path to the backend index containing the points data.

--dataset: Different datasets of points have different characteristics, such as maximum and minimum latitude and longitude, which are embedded in the code. This parameter is used to switch between different points datasets.

--polygonList: Path to the file containing the polygon data.

--polygonDataset: Give a name for the polygon dataset, setting to "neigh" for RasterPIP-i.

--locAttrib: Denote which is the spatial (location) attribute of the record, setting to "1" for RasterPIP-i.

--nAttrib: Number of filtering constraints, setting to "1" for RasterPIP-i.

--startTime and --endTime: Define a time range and retrieve the corresponding data from the backend. For RasterPIP-i, it is the same as RasterJoin. 

--inmem: Execute algorithm in GPU memory.

--gpuMem: GPU memory usage in Megabytes.

--experiments Path to file used to store the experiment data, such as execution time in milliseconds.

--selectFile Path to file used to store the points returned by RasterPIP-i.
