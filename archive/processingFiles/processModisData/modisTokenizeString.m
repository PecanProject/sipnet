function [timeOut,elementName,pixelValues]=modisTokenizeString(stringIn)

%%% Take a modis data string, tokenize it, and return the pixelValues
%%% First two tokens are just the long name and the modis product, followed by the time (in Ayyyyddd),
%%% the site name, time it was analyzed, the element name, and a 49 element vector of results

%%% for generality, the output will all be strings

%%% Out values:
%%% timeOut = a numeric time value (yyyyddd)
%%% elementName = name of the element analyzed (usually Fpar, LAI, and their QC values)
%%% pixelValues = a cellstring of pixel values for the element analyzed

pixelValues=cellstr(char(zeros(49,1)));		%%% Make a cellstring of values for pixels

%%% ignore the first two tokens
[tok,stringIn]=strtok(stringIn,',');
[tok,stringIn]=strtok(stringIn,',');

%%% assign time value
[timeOut,stringIn]=strtok(stringIn,',');
timeOut(1)=[];		%%% Remove the 'A' designation
timeOut=str2num(timeOut);		%%% Convert to a number

%%% Ignore the next three tokens
[tok,stringIn]=strtok(stringIn,',');
[tok,stringIn]=strtok(stringIn,',');

%%% Assign the element Name
[elementName,stringIn]=strtok(stringIn,',');

for i = 1:49
	[pixelValues{i},stringIn]=strtok(stringIn,',');
end

endfunction