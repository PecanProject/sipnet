function split = splitdataI(data)
% PRE: data has 2 dimensions
% Split data into an 3-dimensional array, where split is performed
% based on first element of 2nd dimesion of data
% This element becomes the index of the first dimension of the split
% array and is dropped out of the array
% If smallest element is not 1, an offset is added to every element so
% first array index is 1
% e.g. if data = 
%   0 2 3
%   1 6 7
%   0 4 5
%   1 8 9
% Then split(1,:,:) = 
%   2 3
%   4 5
% And split(2,:,:) = 
%   6 7
%   8 9

  if (ndims(data) ~= 2)
    fprintf('Error: data must have 2 dimensions\n');
    return
  else
    sMin = min(data(:,1)); % minimum index
    sMax = max(data(:,1)); % maximum index
    sOffset = 1 - sMin; % offset in going from data to array index (to start with 1)
    for sIndex=sMin:sMax % loop through indices
        i = find(data(:,1) == sIndex); % find rows with this index
        split(sIndex + sOffset,:,:) = data(i,2:end); % drop first element (the index)
    end
  end
end
