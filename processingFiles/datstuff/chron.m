
dayofyear=load('data/time');

N=length(dayofyear);
timeday=linspace(1/48,((N-0.5)/24),N)';
hourofday=(mod((1:N)-1,24)+0.5)';

ndaypyr=[365,366,365,365,365,366,365,365,365,366,365,365];

nyear=length(ndaypyr);
nhrpyr=24*ndaypyr;
I0(1)=1;
I1(1)=nhrpyr(1);
for i=2:nyear 
    I0(i)=I1(i-1)+1;
    I1(i)=I0(i)+nhrpyr(i)-1;
end
for i=1:nyear
    yr=1991+i-1;
    year(I0(i):I1(i))=linspace(yr,yr+1-1/nhrpyr(i),nhrpyr(i));
end

ystart=7201;  (24*300+1)
year=year(ystart:end)';

save data/dayofyear dayofyear -ascii   %  fix(dayofyear) is 1 on Jan. 1st
save data/timeday timeday -ascii       %  elapsed time for hourly interval midpoints
save data/hourofday hourofday -ascii   %  fix(hourofday) is 0.5 for midnight-1:00AM
save data/year year -ascii             %  fix(year) is the year, for graphing only

