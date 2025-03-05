function leap = isLeapYear(year)
    % return 1 if year is a leap year, otherwise return 0
    
    if (mod(year,4) > 0 || (mod(year,100) == 0 && mod(year,400) > 0))
        leap = 0; % not a leap year
    else
        leap = 1; % a leap year
    end
    
  