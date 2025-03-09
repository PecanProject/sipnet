
dat{1}=nangap(load('~/niwot/climate/tair'));
% dat{2}=nangap(load('data/vpd'));
dat{2}=nangap(load('~/niwot/climate/rhum'));
dat{3}=nangap(load('~/niwot/climate/ppfd'));
dat{4}=nangap(load('~/niwot/climate/tsoil'));
dat{5}=nangap(load('~/niwot/climate/ustar'));
dat{6}=nangap(load('~/niwot/climate/wspd'));
dat{7}=nangap(load('~/niwot/climate/wdir'));
%dat{8}=nangap(load('data/fh2o'));
numvars=7;

potpar=load('~/niwot/climate/potpar');
hour=load('~/niwot/climate/hourofday'); % note: Rob used hour in middle of time step; we use hour at start; shouldn't make a difference
day=load('~/niwot/climate/day'); % note: Rob used continuous (fractional day); we use integer (equivalent to floor(dayofyear))
%time=load('niwot/climate/timeday'); - time no longer used

shour=sin(2*pi*hour/24.);
chour=cos(2*pi*hour/24.);
sday=sin(2*pi*(day-1)/365.25);
cday=cos(2*pi*(day-1)/365.25);


badcount=zeros(length(dat{1}),1);
for i=1:numvars   
    wg{i}=find(~isnan(dat{i}));
    wb{i}=find(isnan(dat{i}));
    nb(i)=length(wb{i});
    badcount(wb{i})=badcount(wb{i})+1;
end

          
x=[shour chour sday cday];
for i=1:numvars datfill{i}=dat{i};end


for i=1:numvars 
    if (nb(i)==0) % no bad points - don't bother filling
        continue; % go to next iteration of loop
    end
    [yg,net,sig]=nnregress(x(wg{i},:),dat{i}(wg{i}),5,1.5,1);
    r2a(i)=net.r2;
    relerra(i)=sqrt(mean((dat{i}(wg{i})-yg).^2))/sqrt(mean(dat{i}(wg{i}).^2));   
    datfill{i}(wb{i})=nnfwd(net,x(wb{i},:));
    datmod{i}=nnfwd(net,x);
end

% toterr=relerr.*nb;

save climfill;

D=cell2mat(datfill);
for i=1:numvars datfill2{i}=datfill{i};end

for i=1:numvars
    if (nb(i)==0) % no bad points - don't bother filling
        continue; % go to next iteration of loop
    end
    vars=setdiff((1:numvars),i);
    X=[x D(:,vars) datmod{i}];
    %X=[x D(:,vars)];
    [yg,net,sig]=nnregress(X(wg{i},:),datfill{i}(wg{i}),7,1.5,1);
    r2b(i)=net.r2;
    relerrb(i)=sqrt(mean((datfill{i}(wg{i})-yg).^2))/sqrt(mean(datfill{i}(wg{i}).^2));   
    datfill2{i}(wb{i})=nnfwd(net,X(wb{i},:));
    datmod2{i}=nnfwd(net,X);
end

save climfill;

tairfill=datfill2{1};
rhumfill=datfill2{2};
parfill=datfill2{3};
tsoilfill=datfill2{4};
ustarfill=datfill2{5};
wspdfill=datfill2{6};
wdirfill=datfill2{7};
%fh2ofill=datfill2{8};

rhumfill(find(rhumfill<0))=0;
parfill(find(parfill<0))=0;
wspdfill(find(wspdfill<0))=0;
wdirfill(find(wdirfill>360 | wdirfill<0))=0;

% BUILD VPRESS, VPD, VPDSOIL
[vpressfill, vpdfill, vpdsoilfill] = calcvpressNiwot(rhumfill, tairfill, tsoilfill);

save ~/niwot/climate/tairfill tairfill -ascii
save ~/niwot/climate/rhumfill rhumfill -ascii
save ~/niwot/climate/parfill parfill -ascii
save ~/niwot/climate/tsoilfill tsoilfill -ascii
save ~/niwot/climate/ustarfill ustarfill -ascii
save ~/niwot/climate/wspdfill wspdfill -ascii
save ~/niwot/climate/wdirfill wdirfill -ascii
%save data/fh2ofill fh2ofill -ascii
save ~/niwot/climate/vpressfill vpressfill -ascii
save ~/niwot/climate/vpdfill vpdfill -ascii
save ~/niwot/climate/vpdsoilfill vpdsoilfill -ascii


