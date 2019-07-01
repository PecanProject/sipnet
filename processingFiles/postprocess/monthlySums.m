function monthlyData = monthlySums(data, years, days, divisions)
    % divide data up into monthly sums
    % monthlyData[i,j,k] gives sum of data in year (i+firstYear-1), month j,
    % division k
    %  where firstYear is first year in years vector
    % PRE: years and days are vectors giving year and julian day of
    % corresponding position in data vector
    %  divisions is a vector giving divisions (1..n) of data into different
    %  categories (length(divisions) == length(data))
    %  (note that divisions can be all ones, in which case all data from a
    %  given month and year are lumped together; otherwise, divisions allows, e.g. separating data into days and nights)
    
    firstYear = years(1);
    numDivisions = max(divisions);
    
    thisYear = firstYear;
    thisMonth = monthFromDay(days(1), firstYear);
    
    ptsLastMonth = true; % did we find pts last month (init. true so start loop)
    while (ptsLastMonth)
        % first and last days of this month:
        [first,last] = daysFromMonth(thisMonth, thisYear);    
        ptsLastMonth = false; % haven't found points yet this month
        for thisDivision = 1:numDivisions
            % points corresponding to this year, month and division:
            pts = find(years == thisYear & days >= first & days <= last & divisions == thisDivision);
            if (length(pts) > 0)
                monthlyData(thisYear-firstYear+1, thisMonth, thisDivision) = sum(data(pts));
                ptsLastMonth = true; % we've found points this month
            end %if
            % fprintf('Year %d, month %d, division %d: found %d points.\n', thisYear, thisMonth, thisDivision, length(pts));
        end %for
        
        thisMonth = thisMonth + 1;
        if (thisMonth > 12)
            thisMonth = 1;
            thisYear = thisYear + 1;
        end
    end %while
        