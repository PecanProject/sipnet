function outVal=gridAverage(input,varargin)

% Author: owner
% Created: Aug 19, 2009
% Purpose: Average out the values of a grid (primarily fPAR or QC data) to find the average values
% 			It is assumed that each column is a pixel, each row a timestamp.
% 			We default to an average over all values if no arguments are specified.	

%%% Types of averaging:
% 'all': average of all cells  
% 'index': average of certain indices (needs a vector of inputs)
% 'gaussian': gaussian weighted average --> Not yet implemented

[row,col]=size(input);



if nargin == 1
	avgType='all';
else
	avgType=deblank(varargin{1});
endif

if strcmp(avgType,'all') == 1
	outVal = nanmean(input')';
elseif strcmp(avgType,'index') == 1
	idxVector=varargin{2};
	outVal = nanmean((input(:,idxVector))')';
else
 disp('I do not know what you want to do');
endif




endfunction
