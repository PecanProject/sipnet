function plotYears(data, days, years, firstYear, lastYear, window, scaleFactor)
% Given a vector of data, and corresponding vector of years
% (where year(i) is the year of data(i))
% Plot data, multiplied by scaleFactor, for each year in a different color, 
% vs. fractional day of year ((JD - 1) + (hour/24)) (given by days vector)
% (value on x-axis gives fractional day at START of time step)
% Plot only data between firstYear and lastYear (if firstYear = -1, start
% at first year in data; if lastYear = -1, end at last year in vector)
% If window > 1, use a sliding mean window for plotting
% NOTE: As of now, can only handle 7 years before we run out of colors!

colors = ['b','g','r','c','m','k','y']; % set up vector to assign colors to ea. line

if (firstYear == -1)
    firstYear = min(years);
end
if (lastYear == -1)
    lastYear = max(years);
end

figure % create a new figure
hold on

% compute sliding mean (if window = 1, dataMean will equal data)
dataMean = slidingMeanAllPtsCentered(data, window);

% loop through years, plotting each line
for yr = firstYear:lastYear
    i = find(years == yr);
    dataYr = dataMean(i); % find subset of data and days array
    daysYr = days(i);
    plot(daysYr, dataYr * scaleFactor, colors(yr - firstYear + 1)); 
end

legend(int2str((firstYear:lastYear)')); % add appropriate legend to figure

end