function L = lshL(w,k,delta)
if nargin<3, delta=0.01;end
if nargin<2, k=10;end
if nargin<1, w=4;end

P1=lshP2(w,1);
L = ceil(log(1/delta)/-log(1-P1^k));

endfunction
