function monthlyMeanData = monthlyMeans(data, years, days, divisions, lengths)
% divide data up into monthly means, using monthlySums function
% monthlyMeanData[i,j,k] gives mean of data in year (i+firstYear-1), month j, division k
%  where firstYear is first year in years vector
% PRE: years and days are vectors giving year and julian day of corresponding position in data vector
%  divisions is a vector giving divisions (1..n) of data into different categories (length(divisions) == length(data))
%  (note that divisions can be all ones, in which case all data from a
%  given month and year are lumped together; otherwise, divisions
%  allows, e.g. separating data into days and nights)
%  lengths gives relative weighting of each data point

dataWeighted = data .* lengths;
dataWeightedMonthly = monthlySums(dataWeighted, years, days, divisions);

% now we have weighted sums - just have to convert to means:
lengthsMonthly = monthlySums(lengths, years, days, divisions); % compute monthly-summed lengths
for yr = 1:length(lengthsMonthly(:,1,1)) % loop over years
    for mo = 1:length(lengthsMonthly(1,:,1)) % loop over months
        for div = 1:length(lengthsMonthly(1,1,:)) % loop over divisions
            % compute the mean for this year, month and division (mean = sum divided by total weight (i.e. length)):
            if (lengthsMonthly(yr,mo,div) == 0)
                monthlyMeanData(yr,mo,div) = 0; % avoid divide by 0
            else
                monthlyMeanData(yr,mo,div) = dataWeightedMonthly(yr,mo,div)/lengthsMonthly(yr,mo,div);
            end
        end
    end
end
end
