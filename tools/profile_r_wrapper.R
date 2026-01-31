# profile_r_wrapper.R
# Benchmarks the "Text -> NetCDF" conversion step of the PEcAn workflow.
# Replicates the I/O load of model2netcdf.SIPNET.R exactly.

if(!require(ncdf4)) install.packages("ncdf4", repos="http://cran.us.r-project.org")

run_benchmark <- function(filename) {
  
  if (!file.exists(filename)) stop(paste("Input file not found:", filename))
  print(paste("Profiling I/O on:", filename))
  
  # ---------------------------------------------------------
  # 1. PARSE & CLEAN OUTPUT
  # ---------------------------------------------------------
  # Robustly read the file, skipping "Notes" and profiling headers
  raw_lines <- readLines(filename)
  header_idx <- grep("^year", raw_lines)
  if (length(header_idx) == 0) stop("Header line starting with 'year' not found.")
  
  # Filter for data lines (start with numbers)
  data_lines <- raw_lines[grep("^[0-9]", raw_lines)]
  print(paste("  - Header found at line", header_idx[1]))
  print(paste("  - Parsed", length(data_lines), "simulation steps"))

  # Load data
  data <- read.table(text = data_lines)
  
  # Map columns exactly as they appear in your sipnet.out
  colnames(data) <- c("year", "day", "time", "plantWoodC", "plantLeafC", "woodCreation",
                      "soil", "microbeC", "coarseRootC", "fineRootC", "litter",
                      "soilWater", "soilWetnessFrac", "snow", "npp", "nee", "cumNEE",
                      "gpp", "rAboveground", "rSoil", "rRoot", "ra", "rh", "rtot",
                      "evapotranspiration", "transpiration", "minN", "soilOrgN", 
                      "litterN", "nVolatilization", "nLeaching")

  # ---------------------------------------------------------
  # 2. BENCHMARK EXECUTION
  # ---------------------------------------------------------
  years <- unique(data$year)
  print(paste("  - Simulation Range:", min(years), "to", max(years), "(", length(years), "years )"))
  
  print("Starting NetCDF write operation (Full PEcAn Variable Set)...")
  start_time <- Sys.time()
  
  for (y in years) {
    sub_data <- data[data$year == y, ]
    
    # Define Dimensions
    dimX <- ncdim_def("lon", "degrees_east", -105.54)
    dimY <- ncdim_def("lat", "degrees_north", 40.03)
    dimT <- ncdim_def("time", "days", 1:nrow(sub_data), unlim=TRUE)
    
    # Define the FULL set of variables from model2netcdf.SIPNET.R
    # (We map input columns to these outputs to simulate the exact I/O volume)
    vars <- list(
      ncvar_def("GPP", "kgC/m2/s", list(dimX, dimY, dimT), -999),
      ncvar_def("NPP", "kgC/m2/s", list(dimX, dimY, dimT), -999),
      ncvar_def("TotalResp", "kgC/m2/s", list(dimX, dimY, dimT), -999),
      ncvar_def("AutoResp", "kgC/m2/s", list(dimX, dimY, dimT), -999),
      ncvar_def("HeteroResp", "kgC/m2/s", list(dimX, dimY, dimT), -999),
      ncvar_def("SoilResp", "kgC/m2/s", list(dimX, dimY, dimT), -999),
      ncvar_def("NEE", "kgC/m2/s", list(dimX, dimY, dimT), -999),
      ncvar_def("AbvGrndWood", "kgC/m2", list(dimX, dimY, dimT), -999),
      ncvar_def("leaf_carbon_content", "kgC/m2", list(dimX, dimY, dimT), -999),
      ncvar_def("TotLivBiom", "kgC/m2", list(dimX, dimY, dimT), -999),
      ncvar_def("TotSoilCarb", "kgC/m2", list(dimX, dimY, dimT), -999),
      ncvar_def("Qle", "W/m2", list(dimX, dimY, dimT), -999),
      ncvar_def("Transp", "kgW/m2/s", list(dimX, dimY, dimT), -999),
      ncvar_def("SoilMoist", "kgW/m2", list(dimX, dimY, dimT), -999),
      ncvar_def("SoilMoistFrac", "frac", list(dimX, dimY, dimT), -999),
      ncvar_def("SWE", "kg/m2", list(dimX, dimY, dimT), -999),
      ncvar_def("LAI", "m2/m2", list(dimX, dimY, dimT), -999),
      ncvar_def("GWBI", "kgC/m2/s", list(dimX, dimY, dimT), -999),
      ncvar_def("AGB", "kgC/m2", list(dimX, dimY, dimT), -999)
    )
    
    # Create File
    nc_name <- paste0(y, ".nc")
    nc <- nc_create(nc_name, vars)
    
    # Write Data (Mapping relevant columns to simulate load)
    # Note: We skip the complex unit conversion math to focus purely on Write Speed.
    ncvar_put(nc, vars[[1]], sub_data$gpp)
    ncvar_put(nc, vars[[2]], sub_data$npp)
    ncvar_put(nc, vars[[3]], sub_data$rtot)
    ncvar_put(nc, vars[[4]], sub_data$rAboveground + sub_data$rRoot)
    ncvar_put(nc, vars[[5]], sub_data$rh)
    ncvar_put(nc, vars[[6]], sub_data$rSoil)
    ncvar_put(nc, vars[[7]], sub_data$nee)
    ncvar_put(nc, vars[[8]], sub_data$plantWoodC)
    ncvar_put(nc, vars[[9]], sub_data$plantLeafC)
    ncvar_put(nc, vars[[10]], sub_data$plantWoodC + sub_data$plantLeafC) 
    ncvar_put(nc, vars[[11]], sub_data$soil)
    ncvar_put(nc, vars[[12]], sub_data$evapotranspiration) 
    ncvar_put(nc, vars[[13]], sub_data$transpiration)
    ncvar_put(nc, vars[[14]], sub_data$soilWater)
    ncvar_put(nc, vars[[15]], sub_data$soilWetnessFrac)
    ncvar_put(nc, vars[[16]], sub_data$snow)
    ncvar_put(nc, vars[[17]], sub_data$plantLeafC) 
    ncvar_put(nc, vars[[18]], sub_data$woodCreation)
    ncvar_put(nc, vars[[19]], sub_data$plantWoodC) 
    nc_close(nc)
  }
  
  end_time <- Sys.time()
  
  # ---------------------------------------------------------
  # 3. REPORTING
  # ---------------------------------------------------------
  duration <- as.numeric(end_time - start_time)
  print("=======================================")
  print(paste("Total Rows Processed: ", nrow(data)))
  print(paste("R Wrapper I/O Time:   ", round(duration, 4), "sec"))
  print("=======================================")
}

# Execute
run_benchmark("sipnet_large.out")