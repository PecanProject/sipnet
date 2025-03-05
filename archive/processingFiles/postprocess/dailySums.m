function dailyData = dailySums(data, days)
% Divide data up into daily sums
% PRE: days is a vector of same length as data, giving some index of the
% different days (these indices need not be unique - they are just required
% to change from day i to day (i+1)
% POST: dailyData[i] gives sum of data in day i

dailyIndex = 1; % index into dailyData vector
thisDay = days(1);
dailyData(1) = 0; % initialize to 0

for i=1:length(data)
    if (days(i) == thisDay) % it's the same day
        dailyData(dailyIndex) = dailyData(dailyIndex) + data(i);
    else % it's the next day
        thisDay = days(i);
        dailyIndex = dailyIndex + 1;
        dailyData(dailyIndex) = data(i);
    end
end
        