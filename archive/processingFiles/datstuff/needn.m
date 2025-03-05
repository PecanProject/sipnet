
neefill=load('~/niwot/fluxes/neefill');
potpar=load('~/niwot/climate/potpar');

q=potpar;
q(find(q>0))=1;
q=diff(q);
q(end+1)=1;
q=abs(q);
w=find(q==1);
i0=1;

for i=1:(2*ndays+1)
    wdn=i0:w(i);
	needns(i)=sum(neefill(wdn));
    i0=w(i)+1;
end

needns=needns';

save ~/niwot/fluxes/needn needns -ascii;

