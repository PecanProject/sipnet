function [y,sig] = nnfwd(net,x,recalc_stats);
%
% function [y,sig] = nnfwd(net,x,recalc_stats);
%

nin=net.nin;
nout=net.nout;
nstd=net.opt.nstd;
meant=net.opt.meant;
stdt=net.opt.stdt;
minx=net.opt.minx;
maxx=net.opt.maxx;
invhess=net.invhess;

if (nargin==3 & recalc_stats==1)
    meanx=mean(x);
    stdx=std(x);
else
    meanx=net.opt.meanx;
    stdx=net.opt.stdx;
end

%addpath('/net/nfs/zurueck/home/braswell/matlab/netlab');

nrep=1+2*(nin-1);

for i=1:nin
	x(:,i) = (x(:,i)-meanx(i))./(nstd*stdx(i));
end

y=mlpfwd(net,x);

if (nargout>1)
       nx=length(x);
       g = mlpderiv(net,x);
       for k = 1:nout
           for n = 1:nx
             grad = g(n,:,k);
	      sig(n,k) = grad*invhess*grad';
	   end
       end
       sig = 2*sqrt(ones(size(sig))./net.beta + sig);
end

save g;

for i=1:nout
	y(:,i) = y(:,i)*(nstd*stdt(i)) + meant(i);
	if (nargout > 1) sig(:,i) = sig(:,i)*(nstd*stdt(i));end
end







