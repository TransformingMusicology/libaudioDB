function Rho = lshRho(w,c)
if nargin<2, c=1;end
if nargin<1, w=4;end

P1 = lshP2(w,1);
P2 = lshP2(w,c);

Rho = log(1/P1) / log(1/P2);

endfunction
