function [y, net, sig] = nnregress(x,t,nhidden,nstd,diag);
% function [y, net, sig] = nnregress(x,t,nhidden,nstd,diag);
% This performs Bayesian nonlinear regression using an artificial
% neural network. Based on NetLib library.

%path(path,'/net/home/bh/braswell/matlab/Netlab');

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

if (nargin < 3 | isempty(nhidden)) nhidden=7;end  % Number of hidden units.
if (nargin < 4 | isempty(nstd)) nstd=2;end        % Scaling factor for normalization
if (nargin < 5 | isempty(diag)) diag=1;end        % whether to print all the info
alpha_init = 0.005;			          % Initial prior hyperparameter
beta_init = 100.;		                  % Initial noise hyperparameter
nouter = 500;				          % Max number of outer loops
ninner = 3;				          % Number of inner loops
toler = [.005 .005 .005 .005];  	          % When to exit outer loop
options = zeros(1,18);			          % Default options vector
options(2) = 1.0e-7;			          % Absolute precision for weights
options(3) = 1.0e-7;			          % Precision for error function
options(14) = 100;			          % Number of cycles in inner loop
options(1) = -1;			          % Suppress warning messages

[nx nin] = size(x);
[nt nout] = size(t);
if (nt ~= nx) error('Incompatible inputs: nx ~= nt');end

meanx=mean(x);
stdx=std(x);
meant=mean(t);
stdt=std(t);

for i=1:nin x(:,i) = (x(:,i)-meanx(i))./(nstd*stdx(i));end
for i=1:nout t(:,i) = (t(:,i)-meant(i))./(nstd*stdt(i));end

% Create and initialize network weight vector.
net = mlp(nin, nhidden, nout, 'linear', alpha_init, beta_init);

nweights = nhidden*(nin+1) + nout*(nhidden+1);
if (diag == 1)
     fprintf(1,'%s\n',date);
     fprintf(1,'nweights= %d\n\n',nweights);
end

% Train using scaled conjugate gradients, re-estimating alpha and beta.
state=ones(1,4);
warning('off');
for k = 1:nouter
  lastwarn('');
  [net options err] = netopt(net, options, x, t, 'scg');
  warnstate=lastwarn;
  if (~isempty(warnstate) | isnan(net.alpha) | isnan(net.beta))
  	y=zeros(size(t));net=0;sig=0;
	net.r2=0;
	fprintf('Warning: no evidence\n');
	warning('backtrace');
	return;
  end
  [net, gamma] = evidence(net, x, t, ninner);
  warnstate=lastwarn;
  if (~isempty(warnstate) | isnan(net.alpha) | isnan(net.beta) | isnan(gamma) | net.alpha>900.)
  	y=zeros(size(t));net=0;sig=0;
	fprintf('Warning: no evidence\n');
	warning('backtrace');
	net.r2=0;
	return;
  end
  state = [state; [net.alpha net.beta gamma err(end)]];
  satisfy = abs(state(end,:)-state(end-1,:))./mean(state) < toler;
  err=err./nx;
  if (diag == 1)
       fprintf(1, 'Cycle %d: ', k);
       fprintf(1, 'alpha=%f ', net.alpha);
       fprintf(1, 'beta=%f ', net.beta);
       fprintf(1, 'gamma=%f ', gamma);
       fprintf(1, 'error=%f ', err(end));
       fprintf(1, 'satisfy=%d %d %d %d\n', satisfy);
  end
  if (sum(satisfy) == 4) break;end
end
if (k == nouter) fprintf(1,'Warning: maximum outer loop iterations reached\n');end


% Forward estimation
y = mlpfwd(net, x);

% Evaluate error bars.
if (nargout > 2)
       hess = mlphess(net, x, t);
       invhess = inv(hess);
       g = mlpderiv(net, x);

       for k = 1:nout
           for n = 1:nx
             grad = g(n,:,k);
	      sig(n,k) = grad*invhess*grad';
	   end
       end
       sig = 2*sqrt(ones(size(sig))./net.beta + sig);
       net.invhess=invhess;
end

% Return to original units
for i=1:nout
	t(:,i) = t(:,i)*(nstd*stdt(i)) + meant(i);
	y(:,i) = y(:,i)*(nstd*stdt(i)) + meant(i);
	if (nargout > 2) sig(:,i) = sig(:,i)*(nstd*stdt(i));end
end

tout=t;

if (diag==1 | diag==2)
	for i=1:nout
		r=corrcoef(t(:,i),y(:,i));
		rsq(i)=r(2)^2;
	end
	net.r2=rsq;
	if (diag==1) fprintf(1,'R^2: %6.4f\n',rsq(i));end
end

% Save the configuration of this regression
net.opt.nstd=nstd;
net.opt.alpha_init=alpha_init;
net.opt.beta_init=beta_init;
net.opt.nouter=nouter;
net.opt.ninner_evi=ninner;
net.opt.ninner_opt=options(14);
net.opt.meant=meant;
net.opt.meanx=meanx;
net.opt.stdt=stdt;
net.opt.stdx=stdx;
net.opt.minx=min(x);
net.opt.maxx=max(x);
