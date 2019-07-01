
% clear('all');
% close('all','hidden');

% nyears=11;

% Harvard forest:
% lat = 42.54;

% Niwot:
lat = 40.03;

% LBA km 83:
% lat = -3.01;

% LBA km 67:
% lat = -2.51;

latRad = lat*(pi/180);
omega = 0.2618; % pi/12

% Harvard forest: (IS THIS RIGHT???)
% hrOffset = -0.267; 

% Niwot:
hrOffset = -0.036; % (Long - CentralMeridian)/15

% LBA km 83:
% hrOffset = 0.361; % (Long - CentralMeridian)/15

% LBA km 67:
% hrOffset = 0.362; % (Long - CentralMeridian)/15

parFrac = 1.0;
sunFrac = 1.0;

nt_nrm=24*365;
nt_leap=24*366;

%NORMAL YEARS
for doy=1:365	
	dayAng = 2*pi*(doy-1)/365;
	eccent = 1.000110 + 0.034221*cos(dayAng) + 0.00128*sin(dayAng) + 0.000719*cos(2*dayAng) + 0.000077*sin(2*dayAng);
	decl = (0.006918 - 0.399912*cos(dayAng) + 0.070257*sin(dayAng) - 0.006758*cos(2*dayAng) + 0.000907*sin(2*dayAng) - 0.002697*cos(3*dayAng) + 0.00148*sin(3*dayAng));
	% for hr=1:24
    % for hr = 0.5:0.5:24 % every half hour - BAD: STARTS AT 0.5, NOT 0
    for hr = 0:0.5:23.5 % every half hour
		% k = (doy-1)*24 + hr;
        k = (doy - 1)*48 + hr*2 + 1;
		% t=mod(hr+hrOffset,24)-12;
        t=mod((hr+0.25)+hrOffset,24)-12; % add 0.25 to put us in the middle of the timestep
		sza_nrm(k) = acos(sin(latRad)*sin(decl) + cos(latRad)*cos(decl)*cos(omega*t));
		rad_nrm(k) = max([0,parFrac*1367*eccent*(cos(decl)*cos(latRad)*cos(omega*t) + sin(decl)*sin(latRad))]);
		parA_nrm(k) = (.18+(.62*sunFrac))*((rad_nrm(k)*0.001*2.05*10000000)/10000);
	end 	
	srise_nrm(doy) = -(acos(-tan(decl)*tan(latRad))/omega); % don't bother doing offset on srise and sset
	sset_nrm(doy) = +(acos(-tan(decl)*tan(latRad))/omega); % because we only care about their difference
    daylen_nrm(doy) = sset_nrm(doy)-srise_nrm(doy);
end
%LEAP YEARS
for doy=1:366
	dayAng = 2*pi*(doy-1)/366;
	eccent = 1.000110 + 0.034221*cos(dayAng) + 0.00128*sin(dayAng) + 0.000719*cos(2*dayAng) + 0.000077*sin(2*dayAng);
	decl = (0.006918 - 0.399912*cos(dayAng) + 0.070257*sin(dayAng) - 0.006758*cos(2*dayAng) + 0.000907*sin(2*dayAng) - 0.002697*cos(3*dayAng) + 0.00148*sin(3*dayAng));
	% for hr=1:24
    for hr = 0.5:0.5:24 % every half hour
		% k = (doy-1)*24 + hr;
        k = (doy - 1)*48 + hr*2;
		% t=mod(hr+hrOffset,24)-12;
        t=mod((hr-0.25)+hrOffset,24)-12; % subtract 0.25 to put us in the middle of the timestep
		sza_leap(k) = acos(sin(latRad)*sin(decl) + cos(latRad)*cos(decl)*cos(omega*t));
		rad_leap(k) = max([0,parFrac*1367*eccent*(cos(decl)*cos(latRad)*cos(omega*t) + sin(decl)*sin(latRad))]);
		parA_leap(k) = (.18+(.62*sunFrac))*((rad_leap(k)*0.001*2.05*10000000)/10000);
	end 	
	srise_leap(doy) = -(acos(-tan(decl)*tan(latRad))/omega); % don't bother doing offset on srise and sset
	sset_leap(doy) = +(acos(-tan(decl)*tan(latRad))/omega); % because we only care about their difference
    daylen_leap(doy) = sset_leap(doy)-srise_leap(doy);
end

% HARVARD FOREST:
% INCLUDED ARE LAST 1560 HOURS OF 1991
% rad_1991=rad_nrm(end-1559:end);
% parA_1991=parA_nrm(end-1559:end);
% daylen_1991=daylen_nrm(end-1559:end);

% radY = [rad_1991';rad_leap';rad_nrm';rad_nrm';rad_nrm';rad_leap';rad_nrm';rad_nrm';rad_nrm';rad_leap';rad_nrm';rad_nrm'];
% parAY = [parA_1991';parA_leap';parA_nrm';parA_nrm';parA_nrm';parA_leap';parA_nrm'; parA_nrm';parA_nrm';parA_leap';parA_nrm';parA_nrm'];
% daylenY = [daylen_1991';daylen_leap';daylen_nrm';daylen_nrm';daylen_nrm';daylen_leap';daylen_nrm';daylen_nrm';daylen_nrm';daylen_leap';daylen_nrm';daylen_nrm'];

% timeY = 1:((24*(365*11 + 3))+1560);

% NIWOT:
% INCLUDED ARE LAST 2928 HALF-HOURS (61 days) OF 1998
rad_1998 = rad_nrm(end-2927:end);
parA_1998 = parA_nrm(end-2927:end);
daylen_1998 = daylen_nrm(end-60:end);

% 365 days for 1999, 2001, 2002, 2003; 366 days for 2000 and 2004; and last 61 days
% of 1998
timeY = 1:(48*(61+365*4+366*2)); 

radY = [rad_1998';rad_nrm';rad_leap';rad_nrm';rad_nrm';rad_nrm';rad_leap'];
parAY = [parA_1998';parA_nrm';parA_leap';parA_nrm';parA_nrm';parA_nrm';parA_leap'];
daylenY = [daylen_1998';daylen_nrm';daylen_leap';daylen_nrm';daylen_nrm';daylen_nrm';daylen_leap'];

% LBA km 83:
% INCLUDED ARE LAST 8880 HALF-HOURS (185 days) OF 2000 AND FIRST 8688
% HALF-HOURS (181 days) OF 2001
% timeY = 1:(48*366);
% radY = [rad_leap(end-8879:end)';rad_nrm(1:8688)'];
% parAY = [parA_leap(end-8879:end)';parA_nrm(1:8688)'];
% daylenY = [daylen_leap(end-184:end)';daylen_nrm(1:181)'];



% FOR CALCULATING STATISTICS TO CHECK POTPAR:
% par=load('niwot/climate/parfill');

% w=find(par > 0 & parAY > 0);

% ratio=(par(w)./parAY(w));
% med=median(ratio);



% ndays=length(parAY)/24;
ndays = length(parAY)/48; % half-hours

for i=1:ndays
    % r=(i-1)*24+1:i*24;
    r = (i-1)*48+1:i*48; % half-hours
    nlight(i)=length(find(parAY(r)>0));
end
nlight=nlight';

% SAVE THE DATA
save('~/niwot/climate/potpar','parAY','-ascii');
save('~/niwot/climate/daylen','daylenY','-ascii');
save('~/niwot/climate/nlight','nlight','-ascii');



% figure;
% plot(timeY,parAY,timeY,par)

% figure;
% xc=xcorr(parAY,par,'coeff');
% plot(xc(87672-2:87672+2),'o-')
% xc(87672-2:87672+2)

% figure;
% plot(parAY,par,'.',[0 2200],[0 2200],'r')

% figure;
% hist(log(ratio));
