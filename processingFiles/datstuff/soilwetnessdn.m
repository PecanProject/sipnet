% Computes dn values for both soilwetness and soilwetnessvalid
% soilwetness: for each time step, take mean over measured values in that
% time step, converted to fraction filled
% soilwetnessvalid: 1 if ANY measured values in that time step, 0 if NONE

soilwetness=load('~/niwot/climate/soilwetness'); 
intervaldn=load('~/niwot/climate/intervaldn'); % hours per step

upperlimit = 0.95; % take 95th percentile of 1/2-hourly soilwetness as max
sorted = sort(soilwetness(find(isfinite(soilwetness))));
maxsoilwetness = sorted(round(upperlimit*length(sorted)))
soilwetnessfrac=soilwetness/maxsoilwetness; % convert to fraction filled

numsteps=length(intervaldn);
start=1;

for i=1:numsteps
    fin=start + 2*intervaldn(i) - 1; % multiply by 2 since each point is 1/2 hour
    measuredpts=find(isfinite(soilwetnessfrac(start:fin))) + (start - 1); % the measured pts in this step
    if (length(measuredpts)>=1) % at least one measured point
        soilwetnessdns(i)=mean(soilwetnessfrac(measuredpts));
        soilwetnessvaliddn(i)=1;
    else % no measured pts
        soilwetnessdns(i)=0; % arbitrary
        soilwetnessvaliddn(i)=0;
    end
     
    start = fin + 1;
end

soilwetnessdns = min(soilwetnessdns, 1); % don't let soilwetness go higher than 1
soilwetnessdns = soilwetnessdns';
soilwetnessvaliddn = soilwetnessvaliddn';

save ~/niwot/climate/soilwetnessdn soilwetnessdns -ascii
save ~/niwot/climate/soilwetnessvaliddn soilwetnessvaliddn -ascii