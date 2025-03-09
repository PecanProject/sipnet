% Bill Sacks
% 10/20/03

function climsteps(stepLength)
% for each of tair, vpd, par, tsoil, ppt, and nee, take hourly data and
% aggregate up to timesteps of given stepLength (in hours)
% (by just taking the mean across a timestep)

% Assumes that (total hours)/length is an integer 
% (if not, throws out last fraction of a time step)

tair = load('data/tairfill');
vpd = load('data/vpdfill');
par = load('data/parfill');
tsoil = load('data/tsoilfill');
ppt = load('data/ppt');
nee = load('data/neefill');

nhours = length(nee);
nsteps = nhours/stepLength;

for i = 1:nsteps
    indices = ((i-1)*stepLength + 1):(i*stepLength);
    % indices counts [1,2,3] then [4,5,6], etc.
    tairstep(i) = mean(tair(indices));
    vpdstep(i) = mean(vpd(indices));
    parstep(i) = sum(par(indices)*3600./1.E6);
    % par is input as sum over entire timestep
    tsoilstep(i) = mean(tsoil(indices));
    pptstep(i) = mean(ppt(indices));
    neestep(i) = sum(nee(indices));
    % nee is input as sum over entire timestep
end

%transpose all matrices:
tairstep = tairstep';
vpdstep = vpdstep';
parstep = parstep';
tsoilstep = tsoilstep';
pptstep = pptstep';
neestep = neestep';

save data/tairstep tairstep -ascii;
save data/vpdstep vpdstep -ascii;
save data/parstep parstep -ascii;
save data/tsoilstep tsoilstep -ascii;
save data/pptstep pptstep -ascii;
save data/neestep neestep -ascii;
