function [cup, annualNee] = getCUP(nee, year, firstYear, lastYear, ptsPerDay)
% Given a vector of NEE data, and corresponding vector of years
% (where year(i) is the year of data(i)),
% where (ptsPerDay) data points make up each day in data and year vectors
% Note: there must be a multiple of ptsPerDay points in each year
% Return a vector of carbon uptake periods (CUP) for each year from
% firstYear to lastYear (# days with nee < 0)
% And return a vector of annualNee for each year from firstYear to lastYear

% first reshape nee if necessary (as needed by splitdata)
[rows cols] = size(nee);
if (rows == 1)
    nee = nee';
end

% and do the same for year vector
[rows cols] = size(year);
if (rows == 1)
    year = year';
end

neeByDay = splitdata(nee,ptsPerDay); % split into days
neeDaily = sum(neeByDay'); % find daily sums (have to take transpose so sum along correct dimension)
yearByDay = splitdata(year,ptsPerDay); % split into days
yearDaily = yearByDay(:,1); % the year of each day is the year of the first hour of the day

for thisYear = firstYear:lastYear
    i = find(yearDaily == thisYear);
    annualNee(thisYear - firstYear + 1) = sum(neeDaily(i));
    cup(thisYear - firstYear + 1) = length(find(neeDaily(i) < 0));
end

end