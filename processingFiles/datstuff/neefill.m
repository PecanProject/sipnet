
nee=nangap(load('data/nee'));
tair=load('data/tairfill');
vpd=load('data/vpdfill');
tsoil=load('data/tsoilfill');
par=load('data/parfill');
ustar=load('data/ustarfill');
fh2o=load('data/fh2ofill');
wspd=load('data/wspdfill');
wdir=load('data/wdirfill');
year=load('data/year');

potpar=load('data/potpar');
hour=load('data/hourofday');
day=load('data/dayofyear');

shour=sin(2*pi*hour/24.);
chour=cos(2*pi*hour/24.);
sday=sin(2*pi*(day-1)/365.25);
cday=cos(2*pi*(day-1)/365.25);

neem1=[NaN;nee(1:end-1)];

% NIGHTTIME NEE (NIGHTTIME RESP)
wg=find(~isnan(nee) & ustar>0.25 & potpar==0);
p=[shour chour sday cday tair vpd tsoil fh2o wspd wdir];
[yg,net,sig]=nnregress(p(wg,:),nee(wg),9,1.5,1);                   
[respmod sigmod]=nnfwd(net,p);

% NIGHTTIME NEE (NIGHTTIME RESP) INCLUDING USTAR
wg=find(~isnan(nee) & ustar>0.25 & potpar==0);
p=[shour chour sday cday tair vpd tsoil fh2o wspd wdir];
[yg,net,sig]=nnregress(p(wg,:),nee(wg),9,1.5,1);                   
[respmod sigmod]=nnfwd(net,p);




pj=[shour(1) chour(1) sday(1) cday(1) tair(1) vpd(1) tsoil(1) fh2o(1) 0.0 wspd(1) wdir(1)];
respmod(1)=nnfwd(net,pj);
for i=2:length(nee)
	pj=[shour(i) chour(i) sday(i) cday(i) tair(i) vpd(i) tsoil(i) fh2o(i) respmod(i-1) wspd(i) wdir(i)];
	respmod(i)=nnfwd(net,pj);
end	


neefill=nee;
neefill(wb)=neemod(wb);
sigfill=sigmod;
sigfill(wg)=0.0;

err=nee-neemod;





save neefill;
save data/neefill neefill -ascii;
save data/neesigfill sigfill -ascii



%%%

neem1=[nee(wg(1));nee(1:end-1)];
wg=find(~isnan(nee) & ~isnan(neem1) & ustar>0.2);
wb=find(isnan(nee) | isnan(neem1) | ustar<=0.2);
pg=[hour(wg) day(wg) tair(wg) par(wg) vpd(wg) tsoil(wg) neem1(wg)];
tg=nee(wg);
[yg,net,sig]=nnregress(pg,tg,7,1.,1);                    % R2=0.881
p=[hour day tair par vpd tsoil neem1];
[neemod sigmod]=nnfwd(net,p);
neefill=nee;
neefill(wb)=neemod(wb);
sigfill=sigmod;
sigfill(wg)=0.0;


%SCREEN OUTLINERS?
%err=tg-yg;
%[sorterr isorterr]=sort(err);
%n=length(err);
%i001=fix(0.001*n);
%i999=fix(0.999*n);
%f=find(sorterr>sorterr(i001) & sorterr<sorterr(i999));
%tg2=tg(isorterr(f));
%pg2=pg(isorterr(f),:);
%[yg,net,sig]=nnregress(pg,tg,7,1.,1);                    % R2=


%SCREEN UNLIKELY POINTS?
%load data/potpar;
%par(find(potpar==0))=0;
%wg=find(~isnan(nee) & ustar>0.2 & ~(potpar==0 & nee<0));
%wb=find(isnan(nee) | ustar<=0.2 | (potpar==0 & nee<0));
%pg=[hour(wg) day(wg) tair(wg) par(wg) vpd(wg) tsoil(wg)];
%tg=nee(wg);
%[yg,net,sig]=nnregress(pg,tg,7,1.,1);                    % R2=
%p=[hour day tair par vpd tsoil];
%[neemod sigmod]=nnfwd(net,p);
%neefill=nee;
%neefill(wb)=neemod(wb);


%%%%% TRY TO PREDICT NEE FROM 1997-2001 BASED ON 1991-1997
%timeyears=load('data/timeyears');
%wg=find(~isnan(nee) & ustar>0.2 & timeyears<1997.0);
%pg=[hour(wg) day(wg) tair(wg) par(wg) vpd(wg) tsoil(wg)];
%tg=nee(wg);
%[yg,net,sig]=nnregress(pg,tg,7,1.,1);                    % R^2: 0.835 
%p=[hour day tair par vpd tsoil];
%[neemod sigmod]=nnfwd(net,p);
%wg2=find(~isnan(nee) & ustar>0.2 & timeyears>=1997.0);
%corrcoef(neemod(wg2),nee(wg2)).^2
%save neefillto1997
%
% R^2=0.814
% NOT BAD!



