clear validpts;
clear validptsdn;
validpts=load('~/niwot/fluxes/fh2ovalid'); % 1 or 0 for each point
intervaldn=load('~/niwot/climate/intervaldn'); % hours per step

numsteps=length(intervaldn);
start=1;

for i=1:numsteps
    fin=start + 2*intervaldn(i) - 1; % multiply by 2 since each point is 1/2 hour
    validptsdn(i) = mean(validpts(start:fin)); % fraction of valid pts in this step
    
    start = fin + 1;
end

validptsdn = validptsdn';

save ~/niwot/fluxes/fh2ovaliddn validptsdn -ascii