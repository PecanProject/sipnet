function vQC=modisFPARqc(v)

% Author: AB, JZ
% Modified: 11/13/09
% Purpose: Graph Quality Control
%          AB notebook has the notes on what the different codes mean

%%% Depending on the flag of the modis data the quality of the measurement is reduced
%%% on the condition by a multiplicative factor.

%%% Size of desired target matrix
numRows=length(v);
numCols=length(v{1});



 
v=char(v);		%%% Reshape into an array -- it goes timestep by timestep, so the first 49 rows are the pixels from time 1, etc

 %%% input is a cell matrix
 vQC=ones(length(v),1);
 

 
 	%%% Test to see if good quality by checking 1st bit
 	index=find(bin2dec(v(:,1:2))==1);		%%% Termed 'good quality'
 	vQC(index)=0.8.*vQC(index);				%%% Reduce factor, but not completely
 
 	index=find(bin2dec(v(:,1:2))==2);		%%% Termed 'not produced, cloud'
 	vQC(index)=0;								%%% Not acceptable data
 
 	index=find(bin2dec(v(:,1:2))==3);		%%% Termed 'not able to produce'
 	vQC(index)=0;								%%% Not acceptable data
 
 
 	%%% Test snow_ice
 	index=find(bin2dec(v(:,3))==1);		%%% Termed 'significant snow detected'
 	vQC(index)=0.5.*vQC(index);				
 
 	%%% Test aerosol
 	index=find(bin2dec(v(:,4))==1);		%%% Termed 'med or hi aerosol on pixel'
 	vQC(index)=0.5.*vQC(index);
 
 	%%% Test cirrus clouds
 	index=find(bin2dec(v(:,5))==1);		%%% Termed 'cirrus clouds present'
 	vQC(index)=0.5.*vQC(index);
 
  	%%% Test adjacent clouds
 	index=find(bin2dec(v(:,6))==1);		%%% Termed 'adjacent clouds detected'
 	vQC(index)=0.5.*vQC(index);
 
  	%%% Test cloud shadow
 	index=find(bin2dec(v(:,7))==1);		%%% Termed 'cloud shadow detected'
 	vQC(index)=0.5.*vQC(index);
 
  	%%% Test user mask
 	index=find(bin2dec(v(:,8))==1);		%%% Termed 'user mask bit-set'
 	vQC(index)=0;
 
%%% Reshape into target matrix
vQC=reshape(vQC,numCols,numRows);		%%% Do the opposite way and then transpose
vQC=vQC';
 endfunction