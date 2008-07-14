function P2 = lshP2(w,c)
if nargin<2, c=1;end
if nargin<1, w=4;end

P2 = 1 - 2*normal_cdf(-w/c) - 2/(sqrt(2*pi)*(w/c)) * ( 1-exp(-w^2/(2*c^2)) );

endfunction
