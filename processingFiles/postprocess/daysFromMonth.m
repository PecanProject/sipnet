function [first,last] = daysFromMonth(month, year)
    % for a given month (1..12) and year
    % return first and last days (julian days) of that year
    
    % number of days per month in non-leap years and leap years:
    nonleapMonths = [31,28,31,30,31,30,31,31,30,31,30,31];
    leapMonths = [31,29,31,30,31,30,31,31,30,31,30,31];
    
    if (isLeapYear(year))
        months = leapMonths;
    else
        months = nonleapMonths;
    end
    
    first = sum(months(1:(month-1))) + 1;
    last = first + months(month) - 1;