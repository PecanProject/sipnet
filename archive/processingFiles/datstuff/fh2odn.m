
fh2ofill=load('~/niwot/fluxes/fh2ofill');
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
	fh2odns(i)=sum(fh2ofill(wdn));
    i0=w(i)+1;
end

fh2odns=fh2odns';

save ~/niwot/fluxes/fh2odn fh2odns -ascii;

