function y=nangap(y,missing,undo);

% function y=nangap(y,missing);
% if (nargin<2) missing= -9999;
% y(find(y==missing))=NaN;

if (nargin<2 | isempty(missing)) missing= -9999;end
if (nargin<3 | undo~='undo') y(find(y==missing))=NaN;end
if (nargin>2 & undo=='undo') y(find(isnan(y)))=missing;end

