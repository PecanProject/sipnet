function validIndices=snowValidate(year,dayOfYear,validIndices)

% Author: JMZ
% Created: June 17, 2010
% Purpose: mask fAPAR data for snow.  This needs to be manually done.
%



% year dayEnd	dayStart
% 1999   117	 337
% 2000   88      310
% 2001   85      330
% 2002   124     328
% 2003   135     319
% 2004   91      324
% 2005   125     330
% 2006   99      331
% 2007   104     324


yearVector=[1999:2007];
snowEnd=[117;88;85;124;135;91;125;99;104];
snowStart=[337;310;330;328;319;324;330;331;324];


for i=1:length(yearVector)

   index=find(yearVector(i)==year);

   currDayOfYear=dayOfYear(index);
   currValidIndices=validIndices(index);
   
   snowPreMelt=find(currDayOfYear < snowEnd(i));
   snowWinter=find(currDayOfYear > snowStart(i));
   
   currValidIndices(snowPreMelt)=0;
   currValidIndices(snowWinter)=0;
   
   validIndices(index)=currValidIndices;
end
   
