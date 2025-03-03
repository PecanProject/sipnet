function processModis
%%% Author: JMZ
%%% Date:	6/13/10
%%% Purpose: Read in MODIS data, average it (with QC) and write it out to a MODIS file
%%% Essentially the same file as fileRead.m, but only has the MODIS part and not SIPNET data
%%% Also writes out the fAPAR std dev for analysis
%%% Also masks the QC variable for when there is snow.

%%% Helpful doc: https://lpdaac.usgs.gov/lpdaac/products/modis_products_table/leaf_area_index_fraction_of_photosynthetically_active_radiation/8_day_l4_global_1km/mod15a2

%%% Need to get the QC control - if the bit value is 248 = not reliable for standard deviation.

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
FparStdDev_1km=zeros(numTotal,49);
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
    [timeOut,elementName,pixelFparStdDev]=modisTokenizeString(stringFparStdDev_1km);
    [timeOut,elementName,pixelFpar]=modisTokenizeString(stringFpar_1km);

	FparExtra_QC_cellstr{i}=pixelFpar_QC';

	yearIn=floor(timeOut/10^3);
	dayIn = rem(timeOut,1000);


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


%%% Replace any of the stdDev with Nan (2.48 = 248 is a bit code saying that it couldn't be determined)
%%% See NASA MOD15A2 page for more details

index=find(FparStdDev_1km>1);
FparStdDev_1km(index)=NaN;

%%% Average the data according to the pixels
towerPixels=[17 18 19 24 25 26 31 32 33];
avgFparStdDev_1km=gridAverage(FparStdDev_1km,'index',towerPixels);
avgFpar_1km=gridAverage(Fpar_1km,'index',towerPixels);
avgFparExtra_QC=gridAverage(FparExtra_QC,'index',towerPixels);


%%% Report the pixel to pixel variation
towerFpar_1km=Fpar_1km(:,towerPixels);
diffFpar=towerFpar_1km-repmat(avgFpar_1km,1,9);
[row,col]=size(diffFpar);
diffFpar=reshape(diffFpar,row*col,1);
%%% report back the absolute difference of pixel to pixel variation
mean(diffFpar)
std(diffFpar)


%%% Mask the QC when there is snow
avgFparExtra_QCsnowMask=snowValidate(yearModis,floor(decdayModis),avgFparExtra_QC);


%%% Export results to a data file
data=[ yearModis decdayModis avgFpar_1km avgFparStdDev_1km avgFparExtra_QC avgFparExtra_QCsnowMask];






dlmwrite('modisData.txt', data,'delimiter', '   ','precision', '%8.3f');


%%%%%%%%%%%%%

 



     
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

function dataOut=dataMake(timeIn,dataIn,timeCompare,indexFlag)
    
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
        currIndex = find(timeCompare(i) <= timeIn & timeIn < timeCompare(i+1)); %%% Go with the beginning of timestep
          if isempty(currIndex)==0
              dataOut(i,1:col)=nanmean(dataIn(currIndex,1:col),1);
          end
    end





