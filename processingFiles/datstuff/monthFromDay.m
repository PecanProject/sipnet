function month = monthFromDay(day, year)
    % for a given day of year (julian day) and year
    % return month (1..12) corresponding to that day
    % PRE: day <= 365 if not leap year, day <= 366 if leap year
    
    % number of days per month in non-leap years and leap years:
    nonleapMonths = [31,28,31,30,31,30,31,31,30,31,30,31];
    leapMonths = [31,29,31,30,31,30,31,31,30,31,30,31];
    
    if (isLeapYear(year))
        months = leapMonths;
    else
        months = nonleapMonths;
    end
    
    month = 1; % start by assuming it's January
    lastDay = months(month); % last day in given month
    while (day > lastDay)
        month = month + 1;
        lastDay = lastDay + months(month);
    end
    % post: day <= lastDay, and month gives correct month