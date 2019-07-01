
tairfill=load('~/niwot/climate/tairfill');
vpdfill=load('~/niwot/climate/vpdfill');
vpdsoilfill=load('~/niwot/climate/vpdsoilfill');
parfill=load('~/niwot/climate/parfill');
tsoilfill=load('~/niwot/climate/tsoilfill');
vpressfill=load('~/niwot/climate/vpressfill'); 
wspdfill=load('~/niwot/climate/wspdfill');
% time=load('~/niwot/climate/timeday');
potpar=load('~/niwot/climate/potpar');
nlight=load('~/niwot/climate/nlight');
% pptday=load('data/pptday');
ppt = load('~/niwot/climate/precipfill'); % for Niwot, we have half-hourly precip

q=potpar;
q(find(q>0))=1;
q=diff(q);
q(end+1)=1;
q=abs(q);
w=find(q==1);
i0=1;

% pptwanklist=[1 fix((1:(2*ndays+1))/2+.5)];

for i=1:(2*ndays+1)
    wdn=i0:w(i);
    intervaldn(i)=length(wdn);
    tairdn(i)=mean(tairfill(wdn));
	vpddn(i)=mean(vpdfill(wdn));
    vpdsoildn(i)=mean(vpdsoilfill(wdn));
    vpressdn(i)=mean(vpressfill(wdn));
    wspddn(i)=mean(wspdfill(wdn));
	% pardn(i)=sum(parfill(wdn)*3600./1.E6);
    pardn(i) = sum(parfill(wdn)*1800./1.E6); % use 1800 multiplier for half-hour time steps
    if (mod(i,2)==1) pardn(i)=0;end
	tsoildn(i)=mean(tsoilfill(wdn));
    % pptdn(i)=pptday(pptwanklist(i))/2.;
    pptdn(i) = sum(ppt(wdn));
    i0=w(i)+1;
end

tairdn=tairdn';
vpddn=vpddn';
vpdsoildn=vpdsoildn';
pardn=pardn';
tsoildn=tsoildn';
vpressdn=vpressdn';
wspddn=wspddn';
pptdn=pptdn';
intervaldn=intervaldn';

save ~/niwot/climate/tairdn tairdn -ascii;
save ~/niwot/climate/vpddn vpddn -ascii;
save ~/niwot/climate/vpdsoildn vpdsoildn -ascii;
save ~/niwot/climate/pardn pardn -ascii;
save ~/niwot/climate/tsoildn tsoildn -ascii;
save ~/niwot/climate/vpressdn vpressdn -ascii;
save ~/niwot/climate/wspddn wspddn -ascii;
save ~/niwot/climate/pptdn pptdn -ascii;
save ~/niwot/climate/intervaldn intervaldn -ascii




