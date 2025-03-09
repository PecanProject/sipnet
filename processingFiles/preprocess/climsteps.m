% Bill Sacks
% 10/20/03
% Modified 7/12/06

function climsteps(stepLength, obsStepLength, directory_clim, directory_fluxes)
% for each of tair, vpd, vpdsoil, vpress, par, tsoil, wspd, ppt, nee,
% fh2o, and soilwetness, take hourly (or half-hourly) data and
% aggregate up to timesteps of given stepLength (in # of
% observational time steps)
% (by just taking the mean across a timestep)

% Also determine valid fraction for nee, fh2o, and soilwetness
  
% Assumes that (total # observational time steps)/stepLength is an integer 
% (if not, throws out last fraction of a time step)

% obsStepLength gives the length of each observational time step (in
% seconds) (e.g. 3600 for harvard, 1800 for niwot)
  
% Input and output are from the given directories
  
  soilwetness_upperlimit = 0.95; % take 95th percentile of 1/2-hourly
                                 % soilwetness as max

  tair = load([directory_clim '/tairfill']);
  disp('loaded tair');
  vpd = load([directory_clim '/vpdfill']);
  disp('loaded vpd');
  vpdsoil = load([directory_clim '/vpdsoilfill']);
  disp('loaded vpdsoil');
  vpress = load([directory_clim '/vpressfill']);
  disp('loaded vpress');
  par = load([directory_clim '/parfill']);
  disp('loaded par');
  tsoil = load([directory_clim '/tsoilfill']);
  disp('loaded tsoil');
  wspd = load([directory_clim '/wspdfill']);
  disp('loaded wspd');
  ppt = load([directory_clim '/precipfill']);
  disp('loaded ppt');
  nee = load([directory_fluxes '/neefill']);
  disp('loaded nee');
  fh2o = load([directory_fluxes '/fh2ofill']);
  disp('loaded fh2o');
  soilwetness = load([directory_clim '/soilwetness']);
  disp('loaded soilwetness');
  neevalid = load([directory_fluxes '/neevalid']); % 1 or 0 for each
                                                   % point
  disp('loaded neevalid');
  fh2ovalid = load([directory_fluxes '/fh2ovalid']); % 1 or 0 for
                                                     % each point
  disp('loaded fh2ovalid');
  
  soilwetness_sorted = ...
      sort(soilwetness(find(isfinite(soilwetness))));
  maxsoilwetness = soilwetness_sorted(round(soilwetness_upperlimit* ...
                                            length(soilwetness_sorted)));
  soilwetnessfrac = soilwetness/maxsoilwetness;
  
  nobssteps = length(nee);
  nsteps = nobssteps/stepLength;

  tairf = fopen([directory_clim '/tairstep'], 'w');
  vpdf = fopen([directory_clim '/vpdstep'], 'w');
  vpdsoilf = fopen([directory_clim '/vpdsoilstep'], 'w');
  vpressf = fopen([directory_clim '/vpressstep'], 'w');
  parf = fopen([directory_clim '/parstep'], 'w');
  tsoilf = fopen([directory_clim '/tsoilstep'], 'w');
  wspdf = fopen([directory_clim '/wspdstep'], 'w');
  pptf = fopen([directory_clim '/pptstep'], 'w');
  neef = fopen([directory_fluxes '/neestep'], 'w');
  fh2of = fopen([directory_fluxes '/fh2ostep'], 'w');
  soilwetnessf = fopen([directory_clim, '/soilwetnessstep'], 'w');
  neevalidf = fopen([directory_fluxes '/neevalidstep'], 'w');
  fh2ovalidf = fopen([directory_fluxes '/fh2ovalidstep'], 'w');
  soilwetnessvalidf = fopen([directory_clim, '/soilwetnessvalidstep'], 'w');
  
  disp('starting loop');
  
  % Note: in the loop, we print to file as we go, rather than saving
  % to vectors and then just printing at the end, to save memory (it
  % gets slow to do the latter with large vectors)
  for i = 1:nsteps
    if (mod(i, 1000) == 0)
      disp(i);
    end
    indices = ((i-1)*stepLength + 1):(i*stepLength);
    % indices counts [1,2,3] then [4,5,6], etc.
    fprintf(tairf, '%.7e\n', mean(tair(indices)));
    fprintf(vpdf, '%.7e\n', mean(vpd(indices)));
    fprintf(vpdsoilf, '%.7e\n', mean(vpdsoil(indices)));
    fprintf(vpressf, '%.7e\n', mean(vpress(indices)));
    fprintf(parf, '%.7e\n', sum(par(indices)*obsStepLength./1.E6));
    % par is input as sum over entire timestep
    fprintf(tsoilf, '%.7e\n', mean(tsoil(indices)));
    fprintf(wspdf, '%.7e\n', mean(wspd(indices)));
    % pptstep(i) = mean(ppt(indices));  % OLD: ppt used to be rate (I guess)
    fprintf(pptf, '%.7e\n', sum(ppt(indices))); 
    % ppt is input as sum over entire timestep
    fprintf(neef, '%.7e\n', sum(nee(indices)));
    % nee is input as sum over entire timestep
    fprintf(fh2of, '%.7e\n', sum(fh2o(indices)));
    % fh2o is input as sum over entire timestep
    fprintf(neevalidf, '%.7e\n', mean(neevalid(indices))); % fraction of valid
                                                           % pts in this step
    fprintf(fh2ovalidf, '%.7e\n', mean(fh2ovalid(indices)));
    
    % soilwetness: handled a little differently:
    soilwetness_measuredpts = find(isfinite(soilwetnessfrac(indices))) ...
        + (indices(1) - 1); % the measured points in this step
    if (length(soilwetness_measuredpts >= 1)) % at least one measured
                                             % point
      fprintf(soilwetnessf, '%.7e\n', ...
              min(mean(soilwetnessfrac(soilwetness_measuredpts)), 1)); % don't let soilwetness go higher than 1
      fprintf(soilwetnessvalidf, '%.7e\n', 1);
    else % no measured points
      fprintf(soilwetnessf, '%.7e\n', 0); % arbitrary
      fprintf(soilwetnessvalidf, '%.7e\n', 0);
    end
  end
  
  fclose(tairf);
  fclose(vpdf);
  fclose(vpdsoilf);
  fclose(vpressf);
  fclose(parf);
  fclose(tsoilf);
  fclose(wspdf);
  fclose(pptf);
  fclose(neef);
  fclose(fh2of);
  fclose(soilwetnessf);
  fclose(neevalidf);
  fclose(fh2ovalidf);
  fclose(soilwetnessvalidf);
