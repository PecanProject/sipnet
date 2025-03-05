function processModisSipnet
%%% Author: JMZ
%%% Date:	1/20/10, modified 5/16/11
%%% Purpose: Read in MODIS data, average it (with QC) and write it out to a MODIS file for use by SIPNET
%%% Copy other sipnet files so we can do a data assimilation
%%% We do a QC mask for snow data
%%% link to data: http://daac.ornl.gov/cgi-bin/MODIS/GR_col5_1/mod_viz.html

%%% helper functions:
%%% snowValidate.m
%%% modisTokenizeString.m
%%% modisFPARqc.m
%%% gridAverage.m

clear -v

fileName='additionalData\modisData\MOD15A2.fn_usniwot1.txt';
%% outVal=dlmread(fileName,',');



fid=fopen(fileName);
count = 0;

%%% Loop through to get the size of things
test1=fgetl(fid);	%%% The first one is a headerline

while(test1~=-1)
	test1=fgetl(fid);	%%% The first one is a headerline
	count++;
end

count--;        %%% Subtract one for the last value

%%% Allocate memory for matrices
numTotal = count/6;	 %%% One each for LAIqc, fparQc, and the measurements, and their std. deviation
decdayModis=zeros(numTotal,1);
yearModis=zeros(numTotal,1);
Fpar_1km=zeros(numTotal,49);
FparExtra_QC_cellstr=cellstr(char(zeros(numTotal,1)));

%%% Restart back from the beginning
frewind(fid);
test1=fgetl(fid);	%%% The first one is a headerline


%%% loop through until we are done
for i=1:numTotal
	stringFparExtra_QC=fgetl(fid);
	stringFparLai_QC=fgetl(fid);
	stringFparStdDev_1km=fgetl(fid);
	stringFpar_1km=fgetl(fid);
	stringLaiStdDev_1km=fgetl(fid);
	stringLai_1km=fgetl(fid);

%%% Tokenize the strings
	[timeOut,elementName,pixelFpar_QC]=modisTokenizeString(stringFparExtra_QC);
	[timeOut,elementName,pixelFpar]=modisTokenizeString(stringFpar_1km);
	[timeOut,elementName,pixelFparStdDev]=modisTokenizeString(stringFparStdDev_1km);

	FparExtra_QC_cellstr{i}=pixelFpar_QC';

	yearIn=floor(timeOut/10^3);
	dayIn = floor((timeOut/10^3-yearIn)*10^3);


	yearModis(i)=yearIn;
	decdayModis(i)=dayIn;
	
	FparResult=str2num(char(pixelFpar))./100;          %%% Divide by 100 to get a result ';
	FparStdDevResult=str2num(char(pixelFparStdDev))./100;
	Fpar_1km(i,:)=FparResult';
	FparStdDev_1km(i,:)=FparStdDevResult';
	
	
end

fclose(fid);

%%% Do the QC on the data
FparExtra_QC=modisFPARqc(FparExtra_QC_cellstr);
%%%%%%%%%%%%%

%%% Replace any of the stdDev with Nan (2.48 = 248 is a bit code saying that it couldn't be determined)
%%% See NASA MOD15A2 page for more details
index=find(FparStdDev_1km>1);
FparStdDev_1km(index)=1;


%%% Average the data according to the pixels
towerPixels=[17 18 19 24 25 26 31 32 33];
avgFpar_1km=gridAverage(Fpar_1km,'index',towerPixels);
avgFparExtra_QC=gridAverage(FparExtra_QC,'index',towerPixels);      
avgFparStdDev_1km=gridAverage(FparStdDev_1km,'index',towerPixels);


%%% Mask the QC when there is snow
avgFparExtra_QCsnowMask=snowValidate(yearModis,floor(decdayModis),avgFparExtra_QC);
      
%%% Load up SIPNET data, variable name niwot
fileName='..\..\Sites\Niwot\niwot12\niwot12.clim';
niwotAllData= load("-ascii",fileName);


 
yearNiwot = niwotAllData(:,2);
dayNiwot = niwotAllData(:,3);
hourNiwot = niwotAllData(:,4);

decdayNiwot = dayNiwot + hourNiwot./24;


calYearModis=zeros(length(decdayModis),1);
calYearNiwot=zeros(length(decdayNiwot),1);

%%% We have more site years of niwotData (1998 - 2007) Leap years were in
%%% 2000 and 2004 and 2008

%%% Modis data begins in 2000.  (See miscFiles/yearIndices.xlsx for index values)

%%% Year    Date add
%%% 1998    0
%%% 1999    365
%%% 2000    730
%%% 2001    1096
%%% 2002    1461
%%% 2003    1826
%%% 2004    2191
%%% 2005    2557
%%% 2006    2922
%%% 2007    3287
%%% 2008    3652
%%% 2009    4018
%%% 2010    4383

dayAdd=[0 365 730 1096 1461 1826 2191 2557 2922 3287 3652 4018];
overallYears=1998:2009;

for i=1:length(overallYears)
    index=find(overallYears(i)==yearModis);
    calYearModis(index)=dayAdd(i)+decdayModis(index);
    
    index=find(overallYears(i)==yearNiwot);
    calYearNiwot(index)=dayAdd(i)+decdayNiwot(index);
end


%%% Make them into big long vectors corresponding to the time
avgFpar_1km=dataMake(calYearModis,avgFpar_1km,calYearNiwot,1);
avgFparExtra_QCsnowMask=dataMake(calYearModis,avgFparExtra_QCsnowMask,calYearNiwot,0);
avgFparStdDev_1km=dataMake(calYearModis,avgFparStdDev_1km,calYearNiwot,1);




%%% Output the data to a file

fileInLoc = '..\..\Sites\Niwot\niwot12\niwot12';
fileOutLoc = '..\..\MODISdata\niwotAllDataMODIS';

niwotData= load("-ascii",[fileInLoc '.dat']);
niwotQC = load("-ascii",[fileInLoc '.valid']);
niwotSigma = load("-ascii",[fileInLoc '.sigma']);
%%% Add on MODIS data to climate file
dataOut = [ niwotData avgFpar_1km];
qcOut = [ niwotQC avgFparExtra_QCsnowMask];
sigmaOut = [ niwotSigma avgFparStdDev_1km];

%%% Make sure we have the old climate file w/o MODIS
dlmwrite([fileOutLoc '.dat'], dataOut,'delimiter', '   ','precision', '%8.4f');
dlmwrite([fileOutLoc '.valid'], qcOut,'delimiter', '   ','precision', '%8.4f');
dlmwrite([fileOutLoc '.sigma'], sigmaOut,'delimiter', '   ','precision', '%8.4f');

%%% Copy remaining files
copyfile([fileInLoc '.spd'],[fileOutLoc '.spd']);
copyfile([fileInLoc '.clim'],[fileOutLoc '.clim']);
copyfile([fileInLoc '.spd'],[fileOutLoc '.spd']);
copyfile([fileInLoc '.param'],[fileOutLoc '.param']);
copyfile([fileInLoc '.param-spatial'],[fileOutLoc '.param-spatial']);
     
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

function dataOut=dataMake(timeIn,dataIn,timeCompare,indexFlag)
    format long;
    %%% yearIn will be a smaller vector than yearCompare
   
    [row,col]=size(dataIn);
    if indexFlag == 1
        dataOut=ones(length(timeCompare),col);      %%% Make matrix of data points -
                                                    %%% We have a choice
                                                    %%% for 1 or zero.
    else
        dataOut=zeros(length(timeCompare),col);
    end
    
    for i=1:length(timeCompare)-1
        currIndex = find( (timeCompare(i) <= timeIn) & (timeIn < timeCompare(i+1))); %%% Go with the beginning of timestep

          if isempty(currIndex)==0
            [i        timeIn(currIndex)]
          [timeCompare(i) timeCompare(i+1)]
              dataOut(i)=nanmean(dataIn(currIndex));
          end
    end

endfunction




