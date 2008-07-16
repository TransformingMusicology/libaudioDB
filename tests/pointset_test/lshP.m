function P2 = lshP(w,c,k)
if nargin<3, k=1;end
if nargin<2, c=1;end
if nargin<1, w=4;end

P2 = 1 - 2*normcdf(-w./c) - 2./(sqrt(2*pi)*(w./c)) .* ( 1-exp(-w.^2./(2*c.^2)) );
if(k~=1)
  P2 = P2.^k;
endif
endfunction
