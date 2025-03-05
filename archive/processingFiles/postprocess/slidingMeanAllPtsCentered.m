% Given data[1..n]
% and window, the size of the window

% return meanData[1..n],
% where, if window is even:
%   meanData[i] = mean(data[i-(window/2):i+(window/2)-1]) for  window/2 < i <= n - (window/2 - 1),
%   and this range is narrowed symmetrically if i outside this range (range
%   kept even)
%   Note that mean around 1st point will be NaN
% if window is odd:
%   meanData[i] = mean(data[i-(window-1)/2:i+(window-1)/2]) for
%   (window-1)/2 < i <= n - (window-1)/2
%   and this range is narrowed symmetrically if i outside this range (range
%   kept odd)

% Difference between this function and slidingMeanAllPts is that here we
% compute mean AROUND given point, NOT mean UP TO given point.

function meanData = slidingMeanAllPtsCentered(data, window)
    % first do start points, with not enough points for given window
    startI = 1; % start at first point in vector
    thisWindow = mod(window,2); % start with a window of 0 if window even, 1 if window odd
    if (thisWindow == 0) % handle window size of 0 specially
        meanData(startI) = NaN;
        startI = startI + 1; % we'll really start with point 2 in vector
        thisWindow = thisWindow + 2;
    end
    for i = startI:floor(window/2) % do start points, with not enough points for given window
        % take mean from first point to a symmetrical point on the other
        % side of i
        meanData(i) = mean(data(1:i+floor((thisWindow - 1)/2)));
        thisWindow = thisWindow + 2; % increase window size for next point
    end
    
    % now do bulk of vector
    for i = (floor(window/2) + 1):(length(data) - floor((window - 1)/2))
        % compute mean around i
        meanData(i) = mean(data(i - floor(window/2):i + floor((window - 1)/2)));
    end
    
    % now do end points, with not enough points for given window
    thisWindow = window - 2; % start with a window size 2 smaller
    for i = (length(data) - floor((window - 1)/2) + 1):length(data)
        % take mean from the last point to a symmetrical point on the other
        % side of i
        meanData(i) = mean(data(i - floor(thisWindow/2):length(data)));
        thisWindow = thisWindow - 2;
    end
end